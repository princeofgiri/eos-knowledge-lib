/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * ekn-broadway-window.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EKN_TYPE_BROADWAY_WINDOW (ekn_broadway_window_get_type ())
G_DECLARE_FINAL_TYPE (EknBroadwayWindow, ekn_broadway_window, EKN, BROADWAY_WINDOW, GtkOffscreenWindow)

GtkWidget     *ekn_broadway_window_new ();

G_END_DECLS
