/* Copyright 2016 Endless Mobile, Inc. */

#include "eknc-domain-private.h"

#include "eknc-database-manager-private.h"
#include "eknc-object-model.h"
#include "eknc-utils.h"
#include "eknc-utils-private.h"

// Consolidate header would be nice
#include <eos-shard/eos-shard-blob.h>
#include <eos-shard/eos-shard-record.h>
#include <eos-shard/eos-shard-dictionary.h>
#include <eos-shard/eos-shard-shard-file.h>
#include <string.h>

GQuark
eknc_domain_error_quark (void)
{
  return g_quark_from_static_string ("eknc-domain-error-quark");
}

// This hash is derived from sha1('link-table'), and for now is the hardcoded
// location of link tables for all shards.
#define LINK_TABLE_ID "4dba9091495e8f277893e0d400e9e092f9f6f551"

/**
 * SECTION:domain
 * @title: Domain
 * @short_description: Grab content for a specific application
 *
 * The domain object handles querying for content for a specific application.
 * Usually you will want to use #EkncEngine rather than use this directly.
 */
struct _EkncDomain
{
  GObject parent_instance;

  gchar *app_id;
  gchar *path;
  GList *subscriptions;

  char *language;

  EkncDatabaseManager *db_manager;
  GMutex db_lock;

  GFile *content_dir;
  GFile *manifest_file;
  // List of EosShardShardFile items
  GSList *shards;
  // List of EosShardDictionary items
  GSList *link_tables;
};

static void initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (EkncDomain,
                         eknc_domain,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

enum {
  PROP_APP_ID = 1,
  PROP_PATH,
  PROP_LANGUAGE,

  NPROPS
};

static GParamSpec *eknc_domain_props[NPROPS] = { NULL, };

static void
eknc_domain_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  EkncDomain *self = EKNC_DOMAIN (object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      g_value_set_string (value, self->app_id);
      break;

    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, self->language);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eknc_domain_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
  EkncDomain *self = EKNC_DOMAIN (object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      g_free (self->app_id);
      self->app_id = g_value_dup_string (value);
      break;

    case PROP_PATH:
      g_free (self->path);
      self->path = g_value_dup_string (value);
      break;

    case PROP_LANGUAGE:
      g_free (self->language);
      self->language = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eknc_domain_finalize (GObject *object)
{
  EkncDomain *self = EKNC_DOMAIN (object);

  g_free (self->app_id);
  g_free (self->path);
  g_free (self->language);

  g_clear_object (&self->db_manager);
  g_mutex_clear (&self->db_lock);
  g_clear_object (&self->content_dir);
  g_clear_object (&self->manifest_file);

  g_list_free_full (self->subscriptions, g_free);

  g_slist_free_full (self->shards, g_object_unref);
  g_slist_free_full (self->link_tables, (GDestroyNotify) eos_shard_dictionary_unref);

  G_OBJECT_CLASS (eknc_domain_parent_class)->finalize (object);
}

static void
eknc_domain_class_init (EkncDomainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = eknc_domain_get_property;
  object_class->set_property = eknc_domain_set_property;
  object_class->finalize = eknc_domain_finalize;

  /**
   * EkncDomain:app-id:
   *
   * The application id of the content this domain should be loading.
   */
  eknc_domain_props[PROP_APP_ID] =
    g_param_spec_string ("app-id", "Application ID",
      "Application ID of the domain content",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EkncDomain:path:
   *
   * Path to the domain content.
   */
  eknc_domain_props[PROP_PATH] =
    g_param_spec_string ("path", "Path",
      "Path to the domain content",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * EkncDomain:language:
   *
   * The language used for the domain, as an ISO 639 locale code.
   */
  eknc_domain_props[PROP_LANGUAGE] =
    g_param_spec_string ("language", "Language",
      "The language used by the domain, as an ISO 639 locale code",
      NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     NPROPS,
                                     eknc_domain_props);
}

static void
eknc_domain_init (EkncDomain *self)
{
}

static gboolean
eknc_domain_setup_link_tables (EkncDomain *self,
                               GError **error)
{
  for (GSList *l = self->shards; l; l = l->next)
    {
      EosShardShardFile *shard = l->data;
      g_autoptr(EosShardRecord) record = eos_shard_shard_file_find_record_by_hex_name (shard, LINK_TABLE_ID);
      if (record == NULL)
        continue;
      EosShardDictionary *dict;
      if (!(dict = eos_shard_blob_load_as_dictionary (record->data, error)))
        return FALSE;
      self->link_tables = g_slist_append (self->link_tables, dict);
    }

  return TRUE;
}

static gboolean
eknc_domain_process_subscription (EkncDomain *self,
                                  GFile *bundle_dir,
                                  JsonArray *json_subscriptions,
                                  const gchar *relative_path,
                                  GCancellable *cancellable,
                                  GError **error)
{
  GError *local_error = NULL;

  g_autoptr(GFile) manifest_file = g_file_get_child (bundle_dir, "manifest.json");
  if (!g_file_query_exists (manifest_file, cancellable))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "You have no manifest.json and are not running from a "
                   "Flatpak bundle. You must install some content");
      return FALSE;
    }

  g_autofree gchar *contents = NULL;
  if (!g_file_load_contents (manifest_file, cancellable, &contents,
                             NULL, NULL, error))
    return FALSE;

  g_autoptr(JsonNode) manifest_node = NULL;
  if (!(manifest_node = json_from_string (contents, error)))
    return FALSE;

  if (!JSON_NODE_HOLDS_OBJECT (manifest_node))
    {
      g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_BAD_MANIFEST,
                   "Manifest file does not hold a json object");
      return FALSE;
    }

  JsonObject *json_manifest = json_node_get_object (manifest_node);

  /* FIXME: Remove this hack */
  if (json_subscriptions)
    {
      JsonArray *json_dbs = json_object_get_array_member (json_manifest, "xapian_databases");
      g_autofree gchar *subscription_path = g_file_get_path (bundle_dir);
      g_autoptr(GList) dbs = json_array_get_elements (json_dbs);
      GList *l;

      for (l = dbs; l; l = g_list_next (l))
        {
          JsonObject *json_db = json_node_get_object (l->data);
          JsonObject *json_subscription = json_object_new ();
          const gchar *json_path = json_object_get_string_member (json_db, "path");
          g_autofree gchar *path = g_build_filename (relative_path, subscription_path, json_path, NULL);

          json_object_set_int_member (json_subscription, "offset",
                                      json_object_get_int_member (json_db, "offset"));
          json_object_set_string_member (json_subscription, "path", path);
          json_array_add_object_element (json_subscriptions, json_subscription);
        }
    }

  JsonNode *shards_node = json_object_get_member (json_manifest, "shards");

  if (shards_node == NULL || !JSON_NODE_HOLDS_ARRAY (shards_node))
    {
      g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_BAD_MANIFEST,
                   "Missing or malformed shards entry in manifest");
      return FALSE;
    }

  JsonArray *shards_array = json_node_get_array (shards_node);
  for (guint i = 0; i < json_array_get_length (shards_array); i++)
    {
      JsonNode *shard_node = json_array_get_element (shards_array, i);
      if (!JSON_NODE_HOLDS_OBJECT (shard_node))
        {
          g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_BAD_MANIFEST,
                       "Malformed shard entry in manifest");
          return FALSE;
        }
      JsonNode *path_node = json_object_get_member (json_node_get_object (shard_node), "path");
      if (path_node == NULL || json_node_get_value_type (path_node) != G_TYPE_STRING)
        {
          g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_BAD_MANIFEST,
                       "Malformed shard entry in manifest");
          return FALSE;
        }

      g_autoptr(GFile) shard_file = g_file_get_child (bundle_dir,
                                                      json_node_get_string (path_node));

      g_autofree gchar *path = g_file_get_path (shard_file);
      EosShardShardFile *shard = g_object_new (EOS_SHARD_TYPE_SHARD_FILE,
                                               "path", path,
                                               NULL);
      self->shards = g_slist_append (self->shards, shard);
    }

  return TRUE;
}

static gboolean
eknc_domain_import_subscriptions (EkncDomain *self,
                                  GFile *subscriptions_dir,
                                  JsonArray *json_subscriptions,
                                  const gchar *relative_path,
                                  GCancellable *cancellable,
                                  GError **error)
{
  g_autoptr(GFileEnumerator) subscriptions_iter = g_file_enumerate_children (subscriptions_dir, "",
                                                                             G_FILE_QUERY_INFO_NONE,
                                                                             cancellable, error);
  while (TRUE)
    {
      GFile *dir = NULL;
      GFileInfo *info = NULL;
      if (!g_file_enumerator_iterate (subscriptions_iter, &info, &dir, cancellable, error))
        return FALSE;
      if (dir == NULL)
        break;
      if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
        continue;
      if (!eknc_domain_process_subscription (self,
                                             dir,
                                             json_subscriptions,
                                             relative_path,
                                             cancellable,
                                             error))
          return FALSE;

      self->subscriptions = g_list_prepend (self->subscriptions, g_strdup (g_file_info_get_name (info)));
    }

  return TRUE;
}

static gboolean
eknc_domain_initable_init (GInitable *initable,
                           GCancellable *cancellable,
                           GError **error)
{
  EkncDomain *self = EKNC_DOMAIN (initable);

  gboolean has_app_id = (self->app_id != NULL && *self->app_id != '\0');
  gboolean has_path = (self->path != NULL && *self->path != '\0');

  if (has_path)
    {
      g_autoptr(GFile) subscription_dir = g_file_new_for_path (self->path);
      self->manifest_file = g_file_get_child (subscription_dir, "manifest.json");

      if (!g_file_query_exists (subscription_dir, cancellable))
        {
          g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_PATH_NOT_FOUND,
                       "You must provide an existing domain path");
          return FALSE;
        }

      if (!eknc_domain_process_subscription (self,
                                             subscription_dir,
                                             NULL,
                                             NULL,
                                             cancellable,
                                             error))
          return FALSE;
    }
  else if (has_app_id)
    {
      g_autofree gchar *manifest_tmp = g_strconcat (g_get_user_data_dir (),
                                                    G_DIR_SEPARATOR_S,
                                                    "ekncontent.",
                                                    self->app_id,
                                                    ".manifest.json",
                                                    NULL);

      self->manifest_file = g_file_new_for_path (manifest_tmp);
      JsonArray *json_subscriptions = json_array_new ();

      /* Find out root relative path from manifest file */
      g_autoptr(GFile) p = g_file_get_parent (self->manifest_file);
      g_autoptr(GString) relative_path = g_string_new ("");
      while (TRUE)
        {
          g_autoptr(GFile) old_p = g_steal_pointer (&p);
          p = g_file_get_parent (old_p);
          if (p == NULL)
            break;
          g_string_append (relative_path, "../");
        }

      /* Import subscriptions from data directory */
      g_autoptr(GFile) subscriptions_dir = NULL;
      self->content_dir = eknc_get_data_dir (self->app_id);
      subscriptions_dir = g_file_get_child (self->content_dir,
                                            "com.endlessm.subscriptions");
      if (g_file_query_exists (subscriptions_dir, NULL))
        {
          if (!eknc_domain_import_subscriptions (self,
                                                 subscriptions_dir,
                                                 json_subscriptions,
                                                 relative_path->str,
                                                 cancellable,
                                                 error))
              return FALSE;
        }

      /* Import subscriptions from extensions directories */
      g_autoptr(GList) extensions_dirs = eknc_get_extensions_dirs (self->app_id);
      for (GList *l = extensions_dirs; l != NULL; l = l->next)
        {
          GFile *extension_dir = l->data;
          if (g_file_query_exists(extension_dir, cancellable))
            {
              if (!eknc_domain_import_subscriptions (self,
                                                     extension_dir,
                                                     json_subscriptions,
                                                     relative_path->str,
                                                     cancellable,
                                                     error))
                  return FALSE;
            }
        }

      g_autoptr(JsonGenerator) json_generator = json_generator_new ();
      g_autoptr(JsonNode) json_root_node = json_node_new (JSON_NODE_OBJECT);
      JsonObject *json_root = json_object_new ();

      json_object_set_array_member (json_root, "xapian_databases", json_subscriptions);
      json_node_take_object (json_root_node, json_root);
      json_generator_set_root (json_generator, json_root_node);
      json_generator_to_file (json_generator, manifest_tmp, error);
    }
  else
    {
      g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_APP_ID_NOT_SET,
                   "You must set an app id or path to initialize a domain object");
      return FALSE;
    }

  if (self->shards == NULL)
    {
      g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_EMPTY,
                   "Content is empty");
      return FALSE;
    }

  g_autofree char *db_path = g_file_get_path (self->manifest_file);
  self->db_manager = eknc_database_manager_new (db_path);
  g_mutex_init (&self->db_lock);

  if (!eknc_utils_parallel_init (self->shards, 0, cancellable, error))
    return FALSE;

  if (!eknc_domain_setup_link_tables (self, error))
    return FALSE;

  return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = eknc_domain_initable_init;
}

static EosShardRecord *
eknc_domain_load_record_from_hash_sync (EkncDomain *self,
                                        const gchar *hash)
{
  for (GSList *l = self->shards; l; l = l->next)
    {
      EosShardRecord *record = eos_shard_shard_file_find_record_by_hex_name (l->data, hash);
      if (record != NULL)
        return record;
    }
  return NULL;
}

/**
 * eknc_domain_get_subscription_id:
 * @self: the domain
 *
 * Gets subscription id of the domain
 *
 * Returns: (transfer none): the subscription id
 *
 * Deprecated: Use eknc_domain_get_subscription_ids() instead.
 */
const gchar *
eknc_domain_get_subscription_id (EkncDomain *self)
{
  g_return_val_if_fail (EKNC_IS_DOMAIN (self), NULL);

  return (self->subscriptions) ? self->subscriptions->data : NULL;
}

/**
 * eknc_domain_get_subscription_ids:
 * @self: the domain
 *
 * Gets subscription IDs belonging to the domain.
 *
 * Returns: (transfer container): a list of the subscription IDs.
 */
gchar * const *
eknc_domain_get_subscription_ids (EkncDomain *self)
{
  g_return_val_if_fail (EKNC_IS_DOMAIN (self), NULL);

  guint num_subscriptions = g_list_length (self->subscriptions);
  gchar **retval = g_new0 (gchar *, num_subscriptions + 1);
  for (guint ix = 0; ix < num_subscriptions; ix++)
    {
      retval[ix] = g_list_nth_data (self->subscriptions, ix);
    }
  return retval;
}

/**
 * eknc_domain_get_shards:
 * @self: the domain
 *
 * Gets the list of shard files in the domain.
 *
 * Returns: (element-type EosShardShardFile) (transfer none): the shards
 */
GSList *
eknc_domain_get_shards (EkncDomain *self)
{
  g_return_val_if_fail (EKNC_IS_DOMAIN (self), NULL);

  return self->shards;
}

/**
 * eknc_domain_test_link:
 * @self: the domain
 * @link: the ekn id of link to check for
 * @error: #GError for error reporting.
 *
 * Attempts to determine if the given link corresponds to content within
 * this domain.
 *
 * Returns: (transfer full) (nullable): Returns an EKN URI to that content if
 * so, and %NULL otherwise.
 */
gchar *
eknc_domain_test_link (EkncDomain *self,
                       const gchar *link,
                       GError **error)
{
  for (GSList *l = self->link_tables; l; l = l->next)
    {
      gchar *resolved = eos_shard_dictionary_lookup_key (l->data, link, error);
      if (resolved != NULL)
        return resolved;
    }
  return NULL;
}

static void
on_metadata_stream_parsed (GObject *source,
                           GAsyncResult *result,
                           gpointer user_data)
{
  JsonParser *parser = JSON_PARSER (source);
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  if (!json_parser_load_from_stream_finish (parser, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  EkncContentObjectModel *model = eknc_object_model_from_json_node (json_parser_get_root (parser), &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, model, g_object_unref);
}

/**
 * eknc_domain_get_object:
 * @self: the domain
 * @id: the ekn id of the object to load
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied.
 * @user_data: (closure): the data to pass to callback function.
 *
 * Asynchronously load an object model for the given ekn_id
 */
void
eknc_domain_get_object (EkncDomain *self,
                        const gchar *id,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
  g_return_if_fail (EKNC_IS_DOMAIN (self));
  g_return_if_fail (id != NULL || *id != '\0');
  g_return_if_fail (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);
  JsonParser *parser = json_parser_new_immutable ();
  g_task_set_task_data (task, parser, g_object_unref);

  const gchar *hash = eknc_utils_id_get_hash (id);
  if (hash == NULL)
    {
      g_task_return_new_error (task, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_ID_NOT_VALID,
                               "Not a valid id %s", id);
      return;
    }

  g_autoptr(EosShardRecord) record = eknc_domain_load_record_from_hash_sync (self, hash);
  if (record == NULL)
    {
      g_task_return_new_error (task, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_ID_NOT_FOUND,
                               "Could not find shard record for id %s", id);
      return;
    }

  g_autoptr(GInputStream) stream = eos_shard_blob_get_stream (record->metadata);
  json_parser_load_from_stream_async (parser, stream, cancellable, on_metadata_stream_parsed, g_steal_pointer (&task));
}

/**
 * eknc_domain_get_object_finish:
 * @self: domain
 * @result: the #GAsyncResult that was provided to the callback.
 * @error: #GError for error reporting.
 *
 * Finish a eknc_domain_get_object call.
 *
 * Returns: (transfer full): the content object model that was fetched
 */
EkncContentObjectModel *
eknc_domain_get_object_finish (EkncDomain *self,
                               GAsyncResult *result,
                               GError **error)
{
  g_return_val_if_fail (EKNC_IS_DOMAIN (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static EkncContentObjectModel *
eknc_domain_get_object_sync (EkncDomain *self,
                             const gchar *id,
                             GCancellable *cancellable,
                             GError **error)
{
  const gchar *hash = eknc_utils_id_get_hash (id);
  if (hash == NULL)
    {
      g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_ID_NOT_VALID,
                   "Not a valid id %s", id);
      return NULL;
    }

  g_autoptr(EosShardRecord) record = eknc_domain_load_record_from_hash_sync (self, hash);
  if (record == NULL)
    {
      g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_ID_NOT_FOUND,
                   "Could not find shard record for id %s", id);
      return NULL;
    }

  g_autoptr(GInputStream) stream = eos_shard_blob_get_stream (record->metadata);

  g_autoptr(JsonParser) parser = json_parser_new_immutable ();
  GError *internal_error = NULL;
  json_parser_load_from_stream (parser, stream, cancellable, &internal_error);
  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      return NULL;
    }

  EkncContentObjectModel *model =
    eknc_object_model_from_json_node (json_parser_get_root (parser), &internal_error);

  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      return NULL;
    }

  return model;
}

typedef struct
{
  EkncDomain *domain;

  EkncQueryObject *query;

  EkncDatabaseManager *db_manager;

  char *fixed_stop_terms;
  char *fixed_spell_terms;
} RequestState;

static void
request_state_free (gpointer data)
{
  RequestState *state = data;

  g_free (state->fixed_stop_terms);
  g_free (state->fixed_spell_terms);

  g_clear_object (&state->domain);
  g_clear_object (&state->query);
  g_clear_object (&state->db_manager);

  g_slice_free (RequestState, state);
}

static void
query_fix_task (GTask *task,
                gpointer source_obj,
                gpointer task_data,
                GCancellable *cancellable)
{
  RequestState *request = g_task_get_task_data (task);
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  g_autoptr(GMutexLocker) db_lock = g_mutex_locker_new (&request->domain->db_lock);

  gboolean result =
    eknc_database_manager_fix_query (request->db_manager,
                                     eknc_query_object_get_search_terms (request->query),
                                     &request->fixed_stop_terms,
                                     &request->fixed_spell_terms,
                                     &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

/**
 * eknc_domain_get_fixed_query:
 * @self: the domain
 * @query: the query object to fix
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied.
 * @user_data: (closure): the data to pass to callback function.
 *
 * Asynchronously queries the associated Xapian database to retrieve a
 * corrected version of the given query object. The corrections can be zero or
 * more of the following:
 *      - the query with its stop words removed
 *      - the query which has had spelling correction applied to it.
 *
 * Note that the spelling correction will be performed on the original
 * search terms string, and not the string with stop words removed.
 */
void
eknc_domain_get_fixed_query (EkncDomain *self,
                             EkncQueryObject *query,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
  g_return_if_fail (EKNC_IS_DOMAIN (self));
  g_return_if_fail (EKNC_IS_QUERY_OBJECT (query));
  g_return_if_fail (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  GTask *task = g_task_new (self, cancellable, callback, user_data);

  RequestState *state = g_slice_new0 (RequestState);
  state->domain = g_object_ref (self);
  state->db_manager = g_object_ref (self->db_manager);
  state->query = g_object_ref (query);

  g_task_set_task_data (task, state, request_state_free);

  g_task_run_in_thread (task, query_fix_task);
  g_object_unref (task);
}

/**
 * eknc_domain_get_fixed_query_finish:
 * @self: domain
 * @result: the #GAsyncResult that was provided to the callback.
 * @error: #GError for error reporting.
 *
 * Finish a eknc_domain_get_fixed_query call.
 *
 * Returns: (transfer full): a new query object with the fixed query.
 */
EkncQueryObject *
eknc_domain_get_fixed_query_finish (EkncDomain *self,
                                    GAsyncResult *result,
                                    GError **error)
{
  g_return_val_if_fail (EKNC_IS_DOMAIN (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  gboolean success = g_task_propagate_boolean (G_TASK (result), error);

  if (success)
    {
      RequestState *request = g_task_get_task_data (G_TASK (result));

      /* If we didn't get a corrected query, we can just reuse the existing query object */
      if (request->fixed_stop_terms == NULL && request->fixed_spell_terms == NULL)
        return g_object_ref (request->query);

      if (request->fixed_stop_terms != NULL && request->fixed_spell_terms != NULL)
        return eknc_query_object_new_from_object (request->query,
                                                  "stopword-free-terms", request->fixed_stop_terms,
                                                  "corrected-terms", request->fixed_spell_terms,
                                                  NULL);
      else if (request->fixed_stop_terms != NULL)
        return eknc_query_object_new_from_object (request->query,
                                                  "stopword-free-terms", request->fixed_stop_terms,
                                                  NULL);
      else
        return eknc_query_object_new_from_object (request->query,
                                                  "corrected-terms", request->fixed_spell_terms,
                                                  NULL);
    }

  return NULL;
}

static void
query_task (GTask *task,
            gpointer source_object,
            gpointer task_data,
            GCancellable *cancellable)
{
  RequestState *state = task_data;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  g_autoptr(GMutexLocker) db_lock = g_mutex_locker_new (&state->domain->db_lock);

  const char *lang = state->domain->language;
  if (lang == NULL || *lang == '\0')
    lang = "none";

  g_autoptr(XapianMSet) results =
    eknc_database_manager_query (state->db_manager, state->query, lang, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  int n_results = xapian_mset_get_size (results);
  int upper_bound = xapian_mset_get_matches_upper_bound (results);

  g_debug (G_STRLOC ": Found %d results (upper bound: %d)\n", n_results, upper_bound);

  GList *models = NULL;

  g_autoptr(XapianMSetIterator) iter = xapian_mset_get_begin (results);
  while (xapian_mset_iterator_next (iter))
    {
      GError *internal_error = NULL;

      XapianDocument *document = xapian_mset_iterator_get_document (iter, &internal_error);
      if (internal_error != NULL)
        {
          g_debug ("INTERNAL: Unable to fetch document from iterator: %s",
                   internal_error->message);
          g_error_free (internal_error);
          continue;
        }

      g_autofree char *object_id = xapian_document_get_data (document);

      g_debug ("Retrieving document object '%s'\n", object_id);

      EkncContentObjectModel *model =
        eknc_domain_get_object_sync (state->domain, object_id, NULL, &internal_error);

      if (internal_error != NULL)
        {
          g_task_return_error (task, internal_error);
          return;
        }

      models = g_list_prepend (models, model);
    }

  g_debug ("Models found: %d of %d matches", g_list_length (models), n_results);

  EkncQueryResults *query_results =
    g_object_new (EKNC_TYPE_QUERY_RESULTS,
                  "upper-bound", upper_bound,
                  "models", g_list_reverse (models),
                  NULL);

  g_task_return_pointer (task, query_results, g_object_unref);
}

/**
 * eknc_domain_query:
 * @self: the domain
 * @query: the query object
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied.
 * @user_data: (closure): the data to pass to callback function.
 *
 * Asynchronously fetch a #EkncQueryResults for a #EkncQueryObject.
 */
void
eknc_domain_query (EkncDomain *self,
                   EkncQueryObject *query,
                   GCancellable *cancellable,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
  g_return_if_fail (EKNC_IS_DOMAIN (self));
  g_return_if_fail (EKNC_IS_QUERY_OBJECT (query));
  g_return_if_fail (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  GTask *task = g_task_new (self, cancellable, callback, user_data);

  RequestState *state = g_slice_new0 (RequestState);
  state->domain = g_object_ref (self);
  state->db_manager = g_object_ref (self->db_manager);
  state->query = g_object_ref (query);

  g_task_set_task_data (task, state, request_state_free);

  g_task_run_in_thread (task, query_task);
  g_object_unref (task);
}

/**
 * eknc_domain_query_finish:
 * @self: domain
 * @result: the #GAsyncResult that was provided to the callback.
 * @error: #GError for error reporting.
 *
 * Finish a eknc_domain_query call.
 *
 * Returns: (transfer full): the results object
 */
EkncQueryResults *
eknc_domain_query_finish (EkncDomain *self,
                          GAsyncResult *result,
                          GError **error)
{
  g_return_val_if_fail (EKNC_IS_DOMAIN (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * eknc_domain_read_uri:
 * @self: domain
 * @uri: the ekn uri to read
 * @bytes: (out) (transfer full) (allow-none): return location for the contents GBytes
 * @mime_type: (out) (transfer full) (allow-none): return location for the content mime type
 * @error: #GError for error reporting
 *
 * Reads the contents of a ekn uri and returns a GBytes of the contents and the
 * contents mime type, if the ekn uri contents was found.
 *
 * Returns: true if the uri was successfully searched for, false if an error occurred
 */
gboolean
eknc_domain_read_uri (EkncDomain *self,
                      const gchar *uri,
                      GBytes **bytes,
                      const gchar **mime_type,
                      GError **error)
{
  g_return_val_if_fail(uri && g_str_has_prefix (uri, "ekn://"), FALSE);
  g_auto(GStrv) tokens = g_strsplit (uri + strlen("ekn://"), "/", -1);
  g_return_val_if_fail(tokens && tokens[0] && tokens[1], FALSE);

  g_autoptr(EosShardRecord) record = NULL;
  /* iterate over all shards until we find a matching record */
  for (GSList *l = self->shards; l && !record; l = g_slist_next (l))
    record = eos_shard_shard_file_find_record_by_hex_name (l->data, tokens[1]);

  if (!record)
    return TRUE;

  g_autoptr(EosShardBlob) blob = eos_shard_blob_ref (record->data);
  /* Use resource, if present */
  if (tokens[2])
    blob = eos_shard_record_lookup_blob (record, tokens[2]);

  if (!blob)
    return TRUE;

  if (mime_type)
    *mime_type = g_strdup (eos_shard_blob_get_content_type (blob));

  if (bytes)
    *bytes = eos_shard_blob_load_contents (blob, error);

  return TRUE;
}

/*< private >
 * eknc_domain_get_for_app_id:
 * @app_id: the domains app id
 * @path: path to the content
 * @language: the language used by the domain
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting
 *
 * Gets a domain object for a given app id. Currently only EKN_VERSION 3 domains
 * are supported, but we may bring back multiple version of our on disk database
 * format in the future.
 *
 * Returns: (transfer full): the newly created domain object
 */
EkncDomain *
eknc_domain_get_for_app_id (const char *app_id,
                            const char *path,
                            const char *language,
                            GCancellable *cancellable,
                            GError **error)
{
  g_autofree gchar *ekn_version = eknc_get_ekn_version (app_id, cancellable, error);

  if (ekn_version == NULL)
    return NULL;

  if (!g_str_equal (ekn_version, "3"))
    {
      g_set_error (error, EKNC_DOMAIN_ERROR, EKNC_DOMAIN_ERROR_UNSUPPORTED_VERSION,
                   "Invalid ekn version for app ID %s: %s", app_id, ekn_version);
      return NULL;
    }

  EkncDomain *domain = g_object_new (EKNC_TYPE_DOMAIN,
                                     "app-id", app_id,
                                     "path", path,
                                     "language", language,
                                     NULL);

  if (!g_initable_init (G_INITABLE (domain), cancellable, error))
    g_clear_pointer (&domain, g_object_unref);

  return domain;
}
