/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copyright 2015 Endless Mobile, Inc. */

#include "eknc-utils.h"
#include "eknc-utils-private.h"

#include <eos-shard/eos-shard-shard-file.h>
#include <stdlib.h>

/**
 * SECTION:utils
 * @title: Utils
 * @short_description: Library utility functions
 *
 * Utility functions for the Endless Knowledge Content library.
 */

GQuark
eknc_content_object_error_quark (void)
{
  return g_quark_from_static_string("eknc-content-object-error-quark");
}

/**
 * eknc_utils_append_gparam_from_json_node:
 *
 * For use instantiating a GObject from json data. Marshals the contents
 * of a JsonNode into a GParameter matching the GParamSpec pspec. Appends
 * that GParameter to a GArray, for eventual use with g_object_newv.
 */
void
eknc_utils_append_gparam_from_json_node (JsonNode *node,
                                         GParamSpec *pspec,
                                         GArray *params)
{
  g_autoptr(GError) error = NULL;
  GParameter param = { NULL, { 0, }};

  if (!node)
    return;

  param.name = pspec->name;
  g_value_init (&param.value, pspec->value_type);

  if (G_IS_PARAM_SPEC_VARIANT (pspec))
    {
      GVariantType *type = G_PARAM_SPEC_VARIANT (pspec)->type;
      GVariant *variant = json_gvariant_deserialize (node,
                                                     g_variant_type_peek_string (type),
                                                     &error);
      g_value_set_variant (&param.value, variant);
    }
  else if (JSON_NODE_HOLDS_VALUE (node))
    {
      GValue value = { 0, };
      GType type = json_node_get_value_type (node);

      // Most of our integer properties are stored in json as strings.
      // ImageObject for example was originally trying to follow
      // https://schema.org/ImageObject for its width and height properties,
      // though in practice it is always a string. May be worth a future cleanup,
      // but for now try to parse if we hit this case.
      if (type == G_TYPE_STRING && (pspec->value_type == G_TYPE_INT ||
                                    pspec->value_type == G_TYPE_UINT))
        {
          g_value_init (&value, G_TYPE_INT);
          g_value_set_int (&value, atoi (json_node_get_string (node)));
        }
      else
        {
          json_node_get_value (node, &value);
        }

      if (g_value_type_transformable (pspec->value_type, type))
        g_value_transform (&value, &param.value);
      else
        g_set_error (&error, EKNC_CONTENT_OBJECT_ERROR, EKNC_CONTENT_OBJECT_ERROR_BAD_FORMAT,
                     "Unexpected type %s", g_type_name (type));
    }
  else
    {
      g_set_error (&error, EKNC_CONTENT_OBJECT_ERROR, EKNC_CONTENT_OBJECT_ERROR_BAD_FORMAT,
                   "Unexpected json value");
    }

  if (error)
    g_critical ("Error parsing model property %s: %s",
                pspec->name,
                error->message);
  else
    g_array_append_val (params, param);
}

/**
 * eknc_utils_free_gparam_array:
 *
 * Frees a GArray of GParameters.
 */
void
eknc_utils_free_gparam_array (GArray *params)
{
  for (gint i = 0; i < params->len; i++) {
    GParameter *param = &g_array_index (params, GParameter, i);
    g_value_unset (&param->value);
  }
  g_array_free (params, TRUE);
}

struct parallel_init_data {
  int n_left;
  GError *error;
  GCancellable *internal_cancel;
};

static void
parallel_init_abort (struct parallel_init_data *data)
{
  g_cancellable_cancel (data->internal_cancel);
}

static void
init_callback (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
  struct parallel_init_data *data = user_data;
  GError *error = NULL;

  data->n_left--;

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (source_object), result, &error))
    {
      if (data->error == NULL)
        g_propagate_error (&data->error, error);

      parallel_init_abort (data);
      return;
    }
}

static void
user_cancelled (GCancellable *cancellable,
                gpointer      user_data)
{
  struct parallel_init_data *data = user_data;

  g_cancellable_set_error_if_cancelled (cancellable, &data->error);
  parallel_init_abort (data);
}

/**
 * eknc_utils_parallel_init:
 * @initables: (element-type GAsyncInitable)
 * @io_priority: The IO priority to init at.
 * @cancellable: A #GCancellable
 * @error: The output error.
 *
 * Synchronously initializes a list of #GAsyncInitables in parallel.
 */
gboolean
eknc_utils_parallel_init (GSList        *initables,
                          int            io_priority,
                          GCancellable  *cancellable,
                          GError       **error)
{
  GMainContext *context = g_main_context_new ();
  g_main_context_push_thread_default (context);

  struct parallel_init_data data = {
    .internal_cancel = g_cancellable_new (),
  };

  if (cancellable)
    g_cancellable_connect (cancellable, G_CALLBACK (user_cancelled), &data, NULL);

  data.n_left = g_slist_length (initables);

  GSList *l;
  for (l = initables; l; l = l->next)
    {
      GAsyncInitable *initable = G_ASYNC_INITABLE (l->data);
      g_async_initable_init_async (initable, io_priority, data.internal_cancel, init_callback, &data);
    }

  while (data.n_left > 0)
    g_main_context_iteration (context, TRUE);

  g_object_unref (data.internal_cancel);
  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);

  if (data.error)
    {
      g_propagate_error (error, data.error);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

/**
 * eknc_default_vfs_set_shards:
 * @shards: (type GSList(EosShardShardFile)): a list of shard objects
 *
 * Set a list of shards in the default GVfs extension point where to lookup
 * ekn:// uris resources.
 *
 * Returns: TRUE on success, FALSE if an error occurred
 */
gboolean
eknc_default_vfs_set_shards (GSList *shards)
{
  GType shard_type = EOS_SHARD_TYPE_SHARD_FILE;
  GVfs *vfs = g_vfs_get_default ();
  GSList *l;

  if (g_strcmp0 (G_OBJECT_TYPE_NAME (vfs), "EknVfs"))
    {
      g_warning ("Default VFS is not a EknVfs, ekn:// uri wont be supported");
      return FALSE;
    }

  for (l = shards; l && l->data; l = g_slist_next (l))
    {
      if (!g_type_is_a (G_OBJECT_TYPE (l->data), shard_type))
        {
          g_warning ("%s is not a EosShardShardFile", G_OBJECT_TYPE_NAME (l->data));
          return FALSE;
        }

    }

  g_object_set (vfs, "shards", shards, NULL);

  return TRUE;
}