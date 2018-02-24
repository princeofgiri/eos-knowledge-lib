/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EknrBoxedSetInfo {
  guint    ref_count;
  gboolean featured;
  char    *ekn_id;
  char    *title;
  GStrv    child_tags;
  GStrv    tags;
} EknrBoxedSetInfo;

EknrBoxedSetInfo * eknr_boxed_set_info_new (gboolean             featured,
                                            const char          *ekn_id,
                                            const char          *title,
                                            GStrv                child_tags,
                                            GStrv                tags);

EknrBoxedSetInfo * eknr_boxed_set_info_copy (EknrBoxedSetInfo *set);

EknrBoxedSetInfo * eknr_boxed_set_info_ref (EknrBoxedSetInfo *set);

void eknr_boxed_set_info_unref (EknrBoxedSetInfo *set);

GType eknr_boxed_set_info_get_type (void);

G_END_DECLS
