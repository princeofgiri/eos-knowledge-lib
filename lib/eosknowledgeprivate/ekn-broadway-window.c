/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * ekn-broadway-window.c
 *
 * Copyright (C) 2018 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Juan Pablo Ugarte <ugarte@endlessm.com>
 *
 */

#include <libsoup/soup.h>
#include "ekn-broadway-window.h"
#include "broadway/broadway-output.h"

struct _EknBroadwayWindow
{
  GtkOffscreenWindow parent;
};

typedef struct
{
  SoupServer *server;
  guint16 port;

  BroadwayOutput *output;
  guint32 serial;

} EknBroadwayWindowPrivate;

enum
{
  PROP_0,
  PROP_PORT,
  N_PROPERTIES
};

enum
{
  ERROR,
  LAST_SIGNAL
};

static GParamSpec *properties[N_PROPERTIES];
static guint ekn_broadway_window_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EknBroadwayWindow, ekn_broadway_window, GTK_TYPE_OFFSCREEN_WINDOW);

#define RESOURCES_PATH "/com/endlessm/knowledge-private/broadway"
#define EBW_PRIVATE(d) ((EknBroadwayWindowPrivate *) ekn_broadway_window_get_instance_private((EknBroadwayWindow*)d))

static void
ekn_broadway_window_init (EknBroadwayWindow *self)
{
  EknBroadwayWindowPrivate *priv = EBW_PRIVATE (self);

  priv->server = soup_server_new (NULL);
  priv->port = 8080;

  priv->output = NULL;
  priv->serial = 0;
}

static void
ekn_broadway_window_dispose (GObject *object)
{
  EknBroadwayWindowPrivate *priv = EBW_PRIVATE (object);

  g_clear_object (&priv->server);

  G_OBJECT_CLASS (ekn_broadway_window_parent_class)->dispose (object);
}

static void
ekn_broadway_window_finalize (GObject *object)
{
  G_OBJECT_CLASS (ekn_broadway_window_parent_class)->finalize (object);
}

static void
ekn_broadway_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EknBroadwayWindowPrivate *priv;

  g_return_if_fail (EKN_IS_BROADWAY_WINDOW (object));
  priv = EBW_PRIVATE (EKN_BROADWAY_WINDOW (object));

  switch (prop_id)
    {
      case PROP_PORT:
        priv->port = g_value_get_uint (value);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ekn_broadway_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EknBroadwayWindowPrivate *priv;

  g_return_if_fail (EKN_IS_BROADWAY_WINDOW (object));
  priv = EBW_PRIVATE (EKN_BROADWAY_WINDOW (object));

  switch (prop_id)
    {
      case PROP_PORT:
        g_value_set_uint (value, priv->port);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
server_response_handler (SoupServer        *server,
                         SoupMessage       *msg,
                         const char        *path,
                         GHashTable        *query,
                         SoupClientContext *client,
                         gpointer           user_data)
{
  gchar *resource_path, *mime_type = NULL;
  gconstpointer data = NULL;
  gsize data_size = 0;
  GBytes *bytes;

  if (msg->method != SOUP_METHOD_GET)
    {
      soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
      return;
    }

  /* Default to client.html */
  if (g_strcmp0 (path, "/") == 0 || g_strcmp0 (path, "/index.html") == 0)
    {
      soup_message_set_redirect (msg, SOUP_STATUS_MOVED_PERMANENTLY, "client.html");
      return;
    }

  resource_path = g_strconcat (RESOURCES_PATH, path, NULL);
  g_message ("%s path=%s resource %s", __func__, path, resource_path);

  /* Lookup resource */
  if ((bytes = g_resources_lookup_data (resource_path, 0, NULL)))
    {
      data = g_bytes_get_data (bytes, &data_size);
      mime_type = g_content_type_guess (path, data, data_size, NULL);
    }

  g_free (resource_path);

  if (!data || !mime_type)
    {
      soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
      return;
    }

  soup_message_set_status (msg, SOUP_STATUS_OK);
  soup_message_set_response (msg, mime_type, SOUP_MEMORY_STATIC, data, data_size);
}


static void
on_websocket_closed (SoupWebsocketConnection *self, gpointer user_data)
{
  g_message ("%s", __func__);
}

static void
on_websocket_error (SoupWebsocketConnection *self,
                    GError                  *error,
                    gpointer                 user_data)
{
  g_message ("%s %s", __func__, error->message);
}

static void
on_websocket_message (SoupWebsocketConnection *self,
                      gint                     type,
                      GBytes                  *message,
                      gpointer                 user_data)
{
  g_message ("%s %d len %"G_GSIZE_FORMAT, __func__, type, g_bytes_get_size (message));
}

static void
server_websocket_handler (SoupServer              *server,
                          SoupWebsocketConnection *connection,
                          const char              *path,
                          SoupClientContext       *client,
                          gpointer                 window)
{
  EknBroadwayWindowPrivate *priv = EBW_PRIVATE (window);
  GIOStream *stream;

  g_message ("%s path=%s", __func__, path);

  g_clear_pointer (&priv->output, broadway_output_free);

  if (!(stream = soup_websocket_connection_get_io_stream (connection)))
    {
      g_message ("%s path=%s no stream", __func__, path);
      return;
    }

  /* Create BroadwayOutput */
  priv->output = broadway_output_new (g_io_stream_get_output_stream (stream),
                                      ++priv->serial);

  /* Create BroadwayInput */
  g_signal_connect (connection, "message", G_CALLBACK (on_websocket_message), NULL);
  g_signal_connect (connection, "error", G_CALLBACK (on_websocket_error), NULL);
  g_signal_connect (connection, "closed", G_CALLBACK (on_websocket_closed), NULL);
}

static void
ekn_broadway_window_constructed (GObject *object)
{
  EknBroadwayWindowPrivate *priv = EBW_PRIVATE (object);
  gchar *protocols[] = {"broadway", NULL};
  gchar *origin = NULL;
  GError *error = NULL;

  /* Listen on port */
  if (!soup_server_listen_local (priv->server, priv->port, 0, &error))
    {
      g_warning ("Unable to listen on port %d: %s", priv->port, error->message);
      g_error_free (error);
      return;
    }

  g_message ("%s Listening on port %d", __func__, priv->port);

  /* Handle HTTP requests */
  soup_server_add_handler (priv->server, "/", server_response_handler, NULL, NULL);

  /* Handle Websocket request */
  soup_server_add_websocket_handler (priv->server, "/socket", origin, NULL,
                                     server_websocket_handler,
                                     g_object_ref (object),
                                     g_object_unref);
}

static void
ekn_broadway_window_class_init (EknBroadwayWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ekn_broadway_window_dispose;
  object_class->finalize = ekn_broadway_window_finalize;
  object_class->set_property = ekn_broadway_window_set_property;
  object_class->get_property = ekn_broadway_window_get_property;
  object_class->constructed = ekn_broadway_window_constructed;

  properties[PROP_PORT] =
    g_param_spec_uint ("port",
                       "TCP port",
                       "Port number to listen",
                       0, G_MAXUINT16,
                       8080,
                       G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/* Public API */

GtkWidget *
ekn_broadway_window_new ()
{
  return (GtkWidget *) g_object_new (EKN_TYPE_BROADWAY_WINDOW, NULL);
}

