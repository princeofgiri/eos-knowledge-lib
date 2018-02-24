/* Copyright 2018 Endless Mobile, Inc. */

#include "eknr-boxed-set-info.h"

EknrBoxedSetInfo *
eknr_boxed_set_info_new (gboolean             featured,
                         const char          *ekn_id,
                         const char          *title,
                         GStrv                child_tags,
                         GStrv                tags)
{
  EknrBoxedSetInfo *set = g_new0 (EknrBoxedSetInfo, 1);

  set->ref_count = 1;
  set->featured = featured;
  set->ekn_id = g_strdup (ekn_id);
  set->title = g_strdup (title);
  set->child_tags = g_strdupv ((GStrv) child_tags);
  set->tags = g_strdupv ((GStrv) tags);

  return set;
}

EknrBoxedSetInfo *
eknr_boxed_set_info_copy (EknrBoxedSetInfo *set)
{
  return eknr_boxed_set_info_new (set->featured,
                                  set->ekn_id,
                                  set->title,
                                  set->child_tags,
                                  set->tags);
}

EknrBoxedSetInfo *
eknr_boxed_set_info_ref (EknrBoxedSetInfo *set)
{
  ++set->ref_count;
  return set;
}

void
eknr_boxed_set_info_unref (EknrBoxedSetInfo *set)
{
  --set->ref_count;

  if (set->ref_count > 0)
    return;

  g_clear_pointer (&set->ekn_id, g_free);
  g_clear_pointer (&set->title, g_free);
  g_clear_pointer (&set->child_tags, g_strfreev);
  g_clear_pointer (&set->tags, g_strfreev);

  g_free (set);
}

/* Boxed type registration for EknrBoxedSetInfo */
GType
eknr_boxed_set_info_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static (g_intern_static_string ("EknrBoxedSetInfo"),
                                            (GBoxedCopyFunc) eknr_boxed_set_info_copy,
                                            (GBoxedFreeFunc) eknr_boxed_set_info_unref);

  return type_id;
}

