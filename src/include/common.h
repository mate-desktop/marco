/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco common types shared by core.h and ui.h */

/*
 * Copyright (C) 2001 Havoc Pennington
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef META_COMMON_H
#define META_COMMON_H

/* Don't include GTK or core headers here */
#include <X11/Xlib.h>
#include <glib.h>

typedef struct _MetaResizePopup MetaResizePopup;

typedef enum
{
  META_FRAME_ALLOWS_DELETE            = 1 << 0,
  META_FRAME_ALLOWS_MENU              = 1 << 1,
  META_FRAME_ALLOWS_MINIMIZE          = 1 << 2,
  META_FRAME_ALLOWS_MAXIMIZE          = 1 << 3,
  META_FRAME_ALLOWS_VERTICAL_RESIZE   = 1 << 4,
  META_FRAME_ALLOWS_HORIZONTAL_RESIZE = 1 << 5,
  META_FRAME_HAS_FOCUS                = 1 << 6,
  META_FRAME_SHADED                   = 1 << 7,
  META_FRAME_STUCK                    = 1 << 8,
  META_FRAME_MAXIMIZED                = 1 << 9,
  META_FRAME_ALLOWS_SHADE             = 1 << 10,
  META_FRAME_ALLOWS_MOVE              = 1 << 11,
  META_FRAME_FULLSCREEN               = 1 << 12,
  META_FRAME_IS_FLASHING              = 1 << 13,
  META_FRAME_ABOVE                    = 1 << 14,
  META_FRAME_TILED_LEFT               = 1 << 15,
  META_FRAME_TILED_RIGHT              = 1 << 16
} MetaFrameFlags;

typedef enum
{
  META_MENU_OP_NONE        = 0,
  META_MENU_OP_DELETE      = 1 << 0,
  META_MENU_OP_MINIMIZE    = 1 << 1,
  META_MENU_OP_UNMAXIMIZE  = 1 << 2,
  META_MENU_OP_MAXIMIZE    = 1 << 3,
  META_MENU_OP_UNSHADE     = 1 << 4,
  META_MENU_OP_SHADE       = 1 << 5,
  META_MENU_OP_UNSTICK     = 1 << 6,
  META_MENU_OP_STICK       = 1 << 7,
  META_MENU_OP_WORKSPACES  = 1 << 8,
  META_MENU_OP_MOVE        = 1 << 9,
  META_MENU_OP_RESIZE      = 1 << 10,
  META_MENU_OP_ABOVE       = 1 << 11,
  META_MENU_OP_UNABOVE     = 1 << 12,
  META_MENU_OP_MOVE_LEFT   = 1 << 13,
  META_MENU_OP_MOVE_RIGHT  = 1 << 14,
  META_MENU_OP_MOVE_UP     = 1 << 15,
  META_MENU_OP_MOVE_DOWN   = 1 << 16,
  META_MENU_OP_RECOVER     = 1 << 17
} MetaMenuOp;

typedef struct _MetaWindowMenu MetaWindowMenu;

typedef void (* MetaWindowMenuFunc) (MetaWindowMenu *menu,
                                     Display        *xdisplay,
                                     Window          client_xwindow,
                                     guint32         timestamp,
                                     MetaMenuOp      op,
                                     int             workspace,
                                     gpointer        data);

/* when changing this enum, there are various switch statements
 * you have to update
 */
typedef enum
{
  META_GRAB_OP_NONE,

  /* Mouse ops */
  META_GRAB_OP_MOVING,
  META_GRAB_OP_RESIZING_SE,
  META_GRAB_OP_RESIZING_S,
  META_GRAB_OP_RESIZING_SW,
  META_GRAB_OP_RESIZING_N,
  META_GRAB_OP_RESIZING_NE,
  META_GRAB_OP_RESIZING_NW,
  META_GRAB_OP_RESIZING_W,
  META_GRAB_OP_RESIZING_E,

  /* Keyboard ops */
  META_GRAB_OP_KEYBOARD_MOVING,
  META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
  META_GRAB_OP_KEYBOARD_RESIZING_S,
  META_GRAB_OP_KEYBOARD_RESIZING_N,
  META_GRAB_OP_KEYBOARD_RESIZING_W,
  META_GRAB_OP_KEYBOARD_RESIZING_E,
  META_GRAB_OP_KEYBOARD_RESIZING_SE,
  META_GRAB_OP_KEYBOARD_RESIZING_NE,
  META_GRAB_OP_KEYBOARD_RESIZING_SW,
  META_GRAB_OP_KEYBOARD_RESIZING_NW,

  /* Alt+Tab */
  META_GRAB_OP_KEYBOARD_TABBING_NORMAL,
  META_GRAB_OP_KEYBOARD_TABBING_DOCK,
  META_GRAB_OP_KEYBOARD_TABBING_NORMAL_ALL_WORKSPACES,

  /* Alt+Esc */
  META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL,
  META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL_ALL_WORKSPACES,
  META_GRAB_OP_KEYBOARD_ESCAPING_DOCK,

  META_GRAB_OP_KEYBOARD_ESCAPING_GROUP,

  /* Alt+F6 */
  META_GRAB_OP_KEYBOARD_TABBING_GROUP,

  META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING,

  /* Frame button ops */
  META_GRAB_OP_CLICKING_MINIMIZE,
  META_GRAB_OP_CLICKING_MAXIMIZE,
  META_GRAB_OP_CLICKING_MAXIMIZE_VERTICAL,
  META_GRAB_OP_CLICKING_MAXIMIZE_HORIZONTAL,
  META_GRAB_OP_CLICKING_UNMAXIMIZE,
  META_GRAB_OP_CLICKING_UNMAXIMIZE_VERTICAL,
  META_GRAB_OP_CLICKING_UNMAXIMIZE_HORIZONTAL,
  META_GRAB_OP_CLICKING_DELETE,
  META_GRAB_OP_CLICKING_MENU,
  META_GRAB_OP_CLICKING_SHADE,
  META_GRAB_OP_CLICKING_UNSHADE,
  META_GRAB_OP_CLICKING_ABOVE,
  META_GRAB_OP_CLICKING_UNABOVE,
  META_GRAB_OP_CLICKING_STICK,
  META_GRAB_OP_CLICKING_UNSTICK
} MetaGrabOp;

typedef enum
{
  META_CURSOR_DEFAULT,
  META_CURSOR_NORTH_RESIZE,
  META_CURSOR_SOUTH_RESIZE,
  META_CURSOR_WEST_RESIZE,
  META_CURSOR_EAST_RESIZE,
  META_CURSOR_SE_RESIZE,
  META_CURSOR_SW_RESIZE,
  META_CURSOR_NE_RESIZE,
  META_CURSOR_NW_RESIZE,
  META_CURSOR_MOVE_OR_RESIZE_WINDOW,
  META_CURSOR_BUSY

} MetaCursor;

typedef enum
{
  META_FOCUS_MODE_CLICK,
  META_FOCUS_MODE_SLOPPY,
  META_FOCUS_MODE_MOUSE
} MetaFocusMode;

typedef enum
{
  META_FOCUS_NEW_WINDOWS_SMART,
  META_FOCUS_NEW_WINDOWS_STRICT
} MetaFocusNewWindows;

typedef enum
{
  META_WRAP_NONE,
  META_WRAP_CLASSIC,
  META_WRAP_TOROIDAL
} MetaWrapStyle;

typedef enum
{
  META_ACTION_TITLEBAR_TOGGLE_SHADE,
  META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE,
  META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE_HORIZONTALLY,
  META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE_VERTICALLY,
  META_ACTION_TITLEBAR_MINIMIZE,
  META_ACTION_TITLEBAR_NONE,
  META_ACTION_TITLEBAR_LOWER,
  META_ACTION_TITLEBAR_MENU,
  META_ACTION_TITLEBAR_LAST
} MetaActionTitlebar;

typedef enum
{
  META_FRAME_TYPE_NORMAL,
  META_FRAME_TYPE_DIALOG,
  META_FRAME_TYPE_MODAL_DIALOG,
  META_FRAME_TYPE_UTILITY,
  META_FRAME_TYPE_MENU,
  META_FRAME_TYPE_BORDER,
  META_FRAME_TYPE_LAST
} MetaFrameType;

typedef enum
{
  /* Create gratuitous divergence from regular
   * X mod bits, to be sure we find bugs
   */
  META_VIRTUAL_SHIFT_MASK    = 1 << 5,
  META_VIRTUAL_CONTROL_MASK  = 1 << 6,
  META_VIRTUAL_ALT_MASK      = 1 << 7,
  META_VIRTUAL_META_MASK     = 1 << 8,
  META_VIRTUAL_SUPER_MASK    = 1 << 9,
  META_VIRTUAL_HYPER_MASK    = 1 << 10,
  META_VIRTUAL_MOD2_MASK     = 1 << 11,
  META_VIRTUAL_MOD3_MASK     = 1 << 12,
  META_VIRTUAL_MOD4_MASK     = 1 << 13,
  META_VIRTUAL_MOD5_MASK     = 1 << 14
} MetaVirtualModifier;

/* Relative directions or sides seem to come up all over the place... */
/* FIXME: Replace
 *   screen.[ch]:MetaScreenDirection,
 *   workspace.[ch]:MetaMotionDirection,
 * with the use of MetaDirection.
 */
typedef enum
{
  META_DIRECTION_LEFT       = 1 << 0,
  META_DIRECTION_RIGHT      = 1 << 1,
  META_DIRECTION_TOP        = 1 << 2,
  META_DIRECTION_BOTTOM     = 1 << 3,

  /* Some aliases for making code more readable for various circumstances. */
  META_DIRECTION_UP         = META_DIRECTION_TOP,
  META_DIRECTION_DOWN       = META_DIRECTION_BOTTOM,

  /* A few more definitions using aliases */
  META_DIRECTION_HORIZONTAL = META_DIRECTION_LEFT | META_DIRECTION_RIGHT,
  META_DIRECTION_VERTICAL   = META_DIRECTION_UP   | META_DIRECTION_DOWN,
} MetaDirection;

/* Sometimes we want to talk about sides instead of directions; note
 * that the values must be as follows or meta_window_update_struts()
 * won't work. Using these values also is a safety blanket since
 * MetaDirection used to be used as a side.
 */
typedef enum
{
  META_SIDE_LEFT            = META_DIRECTION_LEFT,
  META_SIDE_RIGHT           = META_DIRECTION_RIGHT,
  META_SIDE_TOP             = META_DIRECTION_TOP,
  META_SIDE_BOTTOM          = META_DIRECTION_BOTTOM
} MetaSide;

/* Function a window button can have.  Note, you can't add stuff here
 * without extending the theme format to draw a new function and
 * breaking all existing themes.
 */
typedef enum
{
  META_BUTTON_FUNCTION_MENU,
  META_BUTTON_FUNCTION_MINIMIZE,
  META_BUTTON_FUNCTION_MAXIMIZE,
  META_BUTTON_FUNCTION_CLOSE,
  META_BUTTON_FUNCTION_SHADE,
  META_BUTTON_FUNCTION_ABOVE,
  META_BUTTON_FUNCTION_STICK,
  META_BUTTON_FUNCTION_UNSHADE,
  META_BUTTON_FUNCTION_UNABOVE,
  META_BUTTON_FUNCTION_UNSTICK,
  META_BUTTON_FUNCTION_LAST
} MetaButtonFunction;

#define MAX_BUTTONS_PER_CORNER META_BUTTON_FUNCTION_LAST

typedef struct _MetaButtonLayout MetaButtonLayout;
struct _MetaButtonLayout
{
  /* buttons in the group on the left side */
  MetaButtonFunction left_buttons[MAX_BUTTONS_PER_CORNER];
  gboolean left_buttons_has_spacer[MAX_BUTTONS_PER_CORNER];

  /* buttons in the group on the right side */
  MetaButtonFunction right_buttons[MAX_BUTTONS_PER_CORNER];
  gboolean right_buttons_has_spacer[MAX_BUTTONS_PER_CORNER];
};

/* should investigate changing these to whatever most apps use */
#define META_ICON_WIDTH 32
#define META_ICON_HEIGHT 32
#define META_MINI_ICON_WIDTH 16
#define META_MINI_ICON_HEIGHT 16

#define META_DEFAULT_ICON_NAME "window"

#define META_PRIORITY_PREFS_NOTIFY   (G_PRIORITY_DEFAULT_IDLE + 10)
#define META_PRIORITY_WORK_AREA_HINT (G_PRIORITY_DEFAULT_IDLE + 15)

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

/*
 * Placement mode
 */
typedef enum
{
  META_PLACEMENT_MODE_AUTOMATIC,
  META_PLACEMENT_MODE_POINTER,
  META_PLACEMENT_MODE_MANUAL
} MetaPlacementMode;

#endif
