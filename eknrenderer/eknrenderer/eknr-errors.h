/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * EKNR_ERROR:
 *
 * Error doamin for Eknr.
 */
#define EKNR_ERROR (eknr_error ())

GQuark eknr_error (void);

/**
 * EknrError:
 * @EKNR_ERROR_SUBSTITUTION_FAILED: Template substitution failed
 * @EKNR_ERROR_HTML_ESCAPE_FAILED: HTML escaping failed
 *
 * Error codes for the %EKNR_ERROR error domain
 */
typedef enum {
  EKNR_ERROR_SUBSTITUTION_FAILED,
  EKNR_ERROR_HTML_ESCAPE_FAILED,
  EKNR_ERROR_UNKNOWN_LEGACY_SOURCE
} EknrError;

G_END_DECLS
