/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco X error handling */

/*
 * Copyright (C) 2001 Havoc Pennington, error trapping inspired by GDK
 * code copyrighted by the GTK team.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>
#include "errors.h"
#include "display-private.h"
#include <errno.h>
#include <stdlib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

void
meta_error_trap_push (MetaDisplay *display)
{
  gdk_error_trap_push ();
}

void
meta_error_trap_pop (MetaDisplay *display,
                     gboolean     last_request_was_roundtrip)
{
  gdk_error_trap_pop_ignored ();
}

void
meta_error_trap_push_with_return (MetaDisplay *display)
{
  gdk_error_trap_push ();
}

int
meta_error_trap_pop_with_return  (MetaDisplay *display,
                                  gboolean     last_request_was_roundtrip)
{
  return gdk_error_trap_pop ();
}

