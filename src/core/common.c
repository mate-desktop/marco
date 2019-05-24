/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity X window decorations */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003, 2004 Red Hat, Inc.
 * Copyright (C) 2005 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "common.h"

void
meta_frame_borders_clear (MetaFrameBorders *self)
{
  self->visible.top = 0;
  self->visible.bottom = 0;
  self->visible.left = 0;
  self->visible.right = 0;
}
