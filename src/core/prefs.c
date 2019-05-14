/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco preferences */

/*
 * Copyright (C) 2001 Havoc Pennington, Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2006 Elijah Newren
 * Copyright (C) 2008 Thomas Thurman
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
#include "prefs.h"
#include "ui.h"
#include "util.h"
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_REASONABLE_WORKSPACES 36

#define MAX_COMMANDS (32 + NUM_EXTRA_COMMANDS)
#define NUM_EXTRA_COMMANDS 2
#define SCREENSHOT_COMMAND_IDX (MAX_COMMANDS - 2)
#define WIN_SCREENSHOT_COMMAND_IDX (MAX_COMMANDS - 1)

/* If you add a key, it needs updating in init() and in the GSettings
 * notify listener and of course in the .gschema file.
 *
 * Keys which are handled by one of the unified handlers below are
 * not given a name here, because the purpose of the unified handlers
 * is that keys should be referred to exactly once.
 */
#define KEY_GENERAL_SCHEMA "org.mate.Marco.general"
#define KEY_GENERAL_TITLEBAR_FONT "titlebar-font"
#define KEY_GENERAL_NUM_WORKSPACES "num-workspaces"
#define KEY_GENERAL_COMPOSITOR "compositing-manager"
#define KEY_GENERAL_COMPOSITOR_FAST_ALT_TAB "compositing-fast-alt-tab"
#define KEY_GENERAL_CENTER_NEW_WINDOWS "center-new-windows"
#define KEY_GENERAL_ICON_SIZE "icon-size"
#define KEY_GENERAL_ALT_TAB_MAX_COLUMNS "alt-tab-max-columns"

#define KEY_COMMAND_SCHEMA "org.mate.Marco.keybinding-commands"
#define KEY_COMMAND_PREFIX "command-"

#define KEY_SCREEN_BINDINGS_SCHEMA "org.mate.Marco.global-keybindings"

#define KEY_WINDOW_BINDINGS_SCHEMA "org.mate.Marco.window-keybindings"

#define KEY_WORKSPACE_NAME_SCHEMA "org.mate.Marco.workspace-names"
#define KEY_WORKSPACE_NAME_PREFIX "name-"

#define KEY_MATE_INTERFACE_SCHEMA "org.mate.interface"
#define KEY_MATE_INTERFACE_ACCESSIBILITY "accessibility"
#define KEY_MATE_INTERFACE_ENABLE_ANIMATIONS "enable-animations"

#define KEY_MATE_TERMINAL_SCHEMA "org.mate.applications-terminal"
#define KEY_MATE_TERMINAL_COMMAND "exec"

#define KEY_MATE_MOUSE_SCHEMA "org.mate.peripherals-mouse"
#define KEY_MATE_MOUSE_CURSOR_THEME "cursor-theme"
#define KEY_MATE_MOUSE_CURSOR_SIZE "cursor-size"

#define SETTINGS(s) g_hash_table_lookup (settings_schemas, (s))

static GSettings *settings_general;
static GSettings *settings_command;
static GSettings *settings_screen_bindings;
static GSettings *settings_window_bindings;
static GSettings *settings_workspace_names;
static GSettings *settings_mate_interface;
static GSettings *settings_mate_terminal;
static GSettings *settings_mate_mouse;
static GHashTable *settings_schemas;

static GList *changes = NULL;
static guint changed_idle;
static GList *listeners = NULL;

static gboolean use_system_font = FALSE;
static PangoFontDescription *titlebar_font = NULL;
static MetaVirtualModifier mouse_button_mods = Mod1Mask;
static MetaFocusMode focus_mode = META_FOCUS_MODE_CLICK;
static MetaFocusNewWindows focus_new_windows = META_FOCUS_NEW_WINDOWS_SMART;
static gboolean raise_on_click = TRUE;
static gboolean attach_modal_dialogs = FALSE;
static char* current_theme = NULL;
static int num_workspaces = 4;
static MetaWrapStyle wrap_style = META_WRAP_NONE;
static MetaActionTitlebar action_double_click_titlebar = META_ACTION_TITLEBAR_TOGGLE_MAXIMIZE;
static MetaActionTitlebar action_middle_click_titlebar = META_ACTION_TITLEBAR_LOWER;
static MetaActionTitlebar action_right_click_titlebar = META_ACTION_TITLEBAR_MENU;
static gboolean application_based = FALSE;
static gboolean disable_workarounds = FALSE;
static gboolean auto_raise = FALSE;
static gboolean auto_raise_delay = 500;
static gboolean provide_visual_bell = FALSE;
static gboolean bell_is_audible = TRUE;
static gboolean reduced_resources = FALSE;
static gboolean mate_accessibility = FALSE;
static gboolean mate_animations = TRUE;
static char *cursor_theme = NULL;
static int   cursor_size = 24;
static int   icon_size   = META_DEFAULT_ICON_SIZE;
static int   alt_tab_max_columns = META_DEFAULT_ALT_TAB_MAX_COLUMNS;
static gboolean use_force_compositor_manager = FALSE;
static gboolean force_compositor_manager = FALSE;
static gboolean compositing_manager = FALSE;
static gboolean compositing_fast_alt_tab = FALSE;
static gboolean resize_with_right_button = FALSE;
static gboolean show_tab_border = FALSE;
static gboolean center_new_windows = FALSE;
static gboolean force_fullscreen = TRUE;
static gboolean allow_tiling = FALSE;
static gboolean allow_top_tiling = TRUE;
static GList *show_desktop_skip_list = NULL;

static MetaVisualBellType visual_bell_type = META_VISUAL_BELL_FULLSCREEN_FLASH;
static MetaButtonLayout button_layout;

/* The screenshot commands are at the end */
static char *commands[MAX_COMMANDS] = { NULL, };

static char *terminal_command = NULL;

static char *workspace_names[MAX_REASONABLE_WORKSPACES] = { NULL, };

static gboolean handle_preference_update_enum (const gchar *key, GSettings *settings);

static gboolean update_key_binding     (const char *name,
                                        gchar *value);
static gboolean update_command            (const char  *name,
                                           const char  *value);
static gboolean update_workspace_name     (const char  *name,
                                           const char  *value);

static void change_notify (GSettings *settings,
                           gchar *key,
                           gpointer user_data);

static char* settings_key_for_workspace_name (int i);

static void queue_changed (MetaPreference  pref);

#if 0
static void     cleanup_error             (GError **error);
#endif

static void maybe_give_disable_workarounds_warning (void);

static void titlebar_handler (MetaPreference, const gchar*, gboolean*);
static void theme_name_handler (MetaPreference, const gchar*, gboolean*);
static void mouse_button_mods_handler (MetaPreference, const gchar*, gboolean*);
static void button_layout_handler (MetaPreference, const gchar*, gboolean*);
static void show_desktop_skip_list_handler (MetaPreference, const gchar*, gboolean*);

static gboolean update_binding            (MetaKeyPref *binding,
                                           gchar  *value);

static void     init_bindings             (GSettings *);
static void     init_screen_bindings      (void);
static void     init_window_bindings      (void);
static void     init_commands             (void);
static void     init_workspace_names      (void);

static MetaPlacementMode placement_mode = META_PLACEMENT_MODE_AUTOMATIC;

typedef struct
{
  MetaPrefsChangedFunc func;
  gpointer data;
} MetaPrefsListener;

/**
 * The details of one preference which is constrained to be
 * one of a small number of string values-- in other words,
 * an enumeration.
 *
 * We could have done this other ways.  One particularly attractive
 * possibility would have been to represent the entire symbol table
 * as a space-separated string literal in the list of symtabs, so
 * the focus mode enums could have been represented simply by
 * "click sloppy mouse".  However, the simplicity gained would have
 * been outweighed by the bugs caused when the ordering of the enum
 * strings got out of sync with the actual enum statement.  Also,
 * there is existing library code to use this kind of symbol tables.
 *
 * Other things we might consider doing to clean this up in the
 * future include:
 *
 *   - most of the keys begin with the same prefix, and perhaps we
 *     could assume it if they don't start with a slash
 *
 *   - there are several cases where a single identifier could be used
 *     to generate an entire entry, and perhaps this could be done
 *     with a macro.  (This would reduce clarity, however, and is
 *     probably a bad thing.)
 *
 *   - these types all begin with a gchar* (and contain a MetaPreference)
 *     and we can factor out the repeated code in the handlers by taking
 *     advantage of this using some kind of union arrangement.
 */
typedef struct
{
  gchar *key;
  gchar *schema;
  MetaPreference pref;
  gint *target;
} MetaEnumPreference;

typedef struct
{
  gchar *key;
  gchar *schema;
  MetaPreference pref;
  gboolean *target;
  gboolean becomes_true_on_destruction;
} MetaBoolPreference;

typedef struct
{
  gchar *key;
  gchar *schema;
  MetaPreference pref;

  /**
   * A handler.  Many of the string preferences aren't stored as
   * strings and need parsing; others of them have default values
   * which can't be solved in the general case.  If you include a
   * function pointer here, it will be called before the string
   * value is written out to the target variable.
   *
   * The function is passed two arguments: the preference, and
   * the new string as a gchar*.  It returns a gboolean;
   * only if this is true, the listeners will be informed that
   * the preference has changed.
   *
   * This may be NULL.  If it is, see "target", below.
   */
  void (*handler) (MetaPreference pref,
                     const gchar *string_value,
                     gboolean *inform_listeners);

  /**
   * Where to write the incoming string.
   *
   * This must be NULL if the handler is non-NULL.
   * If the incoming string is NULL, no change will be made.
   */
  gchar **target;

} MetaStringPreference;

#define METAINTPREFERENCE_NO_CHANGE_ON_DESTROY G_MININT

typedef struct
{
  gchar *key;
  gchar *schema;
  MetaPreference pref;
  gint *target;
  /**
   * Minimum and maximum values of the integer.
   * If the new value is out of bounds, it will be discarded with a warning.
   */
  gint minimum, maximum;
  /**
   * Value to use if the key is destroyed.
   * If this is METAINTPREFERENCE_NO_CHANGE_ON_DESTROY, it will
   * not be changed when the key is destroyed.
   */
  gint value_if_destroyed;
} MetaIntPreference;

/* FIXMEs: */
/* @@@ Don't use NULL lines at the end; glib can tell you how big it is */
/* @@@ /apps/marco/general should be assumed if first char is not / */
/* @@@ Will it ever be possible to merge init and update? If not, why not? */

static MetaEnumPreference preferences_enum[] =
  {
    { "focus-new-windows",
      KEY_GENERAL_SCHEMA,
      META_PREF_FOCUS_NEW_WINDOWS,
      (gint *) &focus_new_windows,
    },
    { "focus-mode",
      KEY_GENERAL_SCHEMA,
      META_PREF_FOCUS_MODE,
      (gint *) &focus_mode,
    },
    { "wrap-style",
      KEY_GENERAL_SCHEMA,
      META_PREF_WRAP_STYLE,
      (gint *) &wrap_style,
    },
    { "visual-bell-type",
      KEY_GENERAL_SCHEMA,
      META_PREF_VISUAL_BELL_TYPE,
      (gint *) &visual_bell_type,
    },
    { "action-double-click-titlebar",
      KEY_GENERAL_SCHEMA,
      META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR,
      (gint *) &action_double_click_titlebar,
    },
    { "action-middle-click-titlebar",
      KEY_GENERAL_SCHEMA,
      META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR,
      (gint *) &action_middle_click_titlebar,
    },
    { "action-right-click-titlebar",
      KEY_GENERAL_SCHEMA,
      META_PREF_ACTION_RIGHT_CLICK_TITLEBAR,
      (gint *) &action_right_click_titlebar,
    },
    { "placement-mode",
      KEY_GENERAL_SCHEMA,
      META_PREF_PLACEMENT_MODE,
      (gint *) &placement_mode,
    },
    { NULL, NULL, 0, NULL },
  };

static MetaBoolPreference preferences_bool[] =
  {
    { "raise-on-click",
      KEY_GENERAL_SCHEMA,
      META_PREF_RAISE_ON_CLICK,
      &raise_on_click,
      TRUE,
    },
    { "titlebar-uses-system-font",
      KEY_GENERAL_SCHEMA,
      META_PREF_TITLEBAR_FONT, /* note! shares a pref */
      &use_system_font,
      TRUE,
    },
    { "application-based",
      KEY_GENERAL_SCHEMA,
      META_PREF_APPLICATION_BASED,
      NULL, /* feature is known but disabled */
      FALSE,
    },
    { "disable-workarounds",
      KEY_GENERAL_SCHEMA,
      META_PREF_DISABLE_WORKAROUNDS,
      &disable_workarounds,
      FALSE,
    },
    { "auto-raise",
      KEY_GENERAL_SCHEMA,
      META_PREF_AUTO_RAISE,
      &auto_raise,
      FALSE,
    },
    { "visual-bell",
      KEY_GENERAL_SCHEMA,
      META_PREF_VISUAL_BELL,
      &provide_visual_bell, /* FIXME: change the name: it's confusing */
      FALSE,
    },
    { "audible-bell",
      KEY_GENERAL_SCHEMA,
      META_PREF_AUDIBLE_BELL,
      &bell_is_audible, /* FIXME: change the name: it's confusing */
      FALSE,
    },
    { "reduced-resources",
      KEY_GENERAL_SCHEMA,
      META_PREF_REDUCED_RESOURCES,
      &reduced_resources,
      FALSE,
    },
    { "accessibility",
      KEY_MATE_INTERFACE_SCHEMA,
      META_PREF_MATE_ACCESSIBILITY,
      &mate_accessibility,
      FALSE,
    },
    { "enable-animations",
      KEY_MATE_INTERFACE_SCHEMA,
      META_PREF_MATE_ANIMATIONS,
      &mate_animations,
      TRUE,
    },
    { KEY_GENERAL_COMPOSITOR,
      KEY_GENERAL_SCHEMA,
      META_PREF_COMPOSITING_MANAGER,
      &compositing_manager,
      FALSE,
    },
    { "compositing-fast-alt-tab",
      KEY_GENERAL_SCHEMA,
      META_PREF_COMPOSITING_FAST_ALT_TAB,
      &compositing_fast_alt_tab,
      FALSE,
    },
    { "resize-with-right-button",
      KEY_GENERAL_SCHEMA,
      META_PREF_RESIZE_WITH_RIGHT_BUTTON,
      &resize_with_right_button,
      FALSE,
    },
    { "show-tab-border",
      KEY_GENERAL_SCHEMA,
      META_PREF_SHOW_TAB_BORDER,
      &show_tab_border,
      FALSE,
    },
    { "center-new-windows",
      KEY_GENERAL_SCHEMA,
      META_PREF_CENTER_NEW_WINDOWS,
      &center_new_windows,
      FALSE,
    },
    { "allow-tiling",
      KEY_GENERAL_SCHEMA,
      META_PREF_ALLOW_TILING,
      &allow_tiling,
      FALSE,
    },
    { "allow-top-tiling",
      KEY_GENERAL_SCHEMA,
      META_PREF_ALLOW_TOP_TILING,
      &allow_top_tiling,
      FALSE,
    },
    { NULL, NULL, 0, NULL, FALSE },
  };

static MetaStringPreference preferences_string[] =
  {
    { "mouse-button-modifier",
      KEY_GENERAL_SCHEMA,
      META_PREF_MOUSE_BUTTON_MODS,
      mouse_button_mods_handler,
      NULL,
    },
    { "theme",
      KEY_GENERAL_SCHEMA,
      META_PREF_THEME,
      theme_name_handler,
      NULL,
    },
    { KEY_GENERAL_TITLEBAR_FONT,
      KEY_GENERAL_SCHEMA,
      META_PREF_TITLEBAR_FONT,
      titlebar_handler,
      NULL,
    },
    { KEY_MATE_TERMINAL_COMMAND,
      KEY_MATE_TERMINAL_SCHEMA,
      META_PREF_TERMINAL_COMMAND,
      NULL,
      &terminal_command,
    },
    { "button-layout",
      KEY_GENERAL_SCHEMA,
      META_PREF_BUTTON_LAYOUT,
      button_layout_handler,
      NULL,
    },
    { "cursor-theme",
      KEY_MATE_MOUSE_SCHEMA,
      META_PREF_CURSOR_THEME,
      NULL,
      &cursor_theme,
    },
    { "show-desktop-skip-list",
      KEY_GENERAL_SCHEMA,
      META_PREF_SHOW_DESKTOP_SKIP_LIST,
      &show_desktop_skip_list_handler,
      NULL,
    },
    { NULL, NULL, 0, NULL, NULL },
  };

static MetaIntPreference preferences_int[] =
  {
    { "num-workspaces",
      KEY_GENERAL_SCHEMA,
      META_PREF_NUM_WORKSPACES,
      &num_workspaces,
      /* I would actually recommend we change the destroy value to 4
       * and get rid of METAINTPREFERENCE_NO_CHANGE_ON_DESTROY entirely.
       *  -- tthurman
       */
      1, MAX_REASONABLE_WORKSPACES, METAINTPREFERENCE_NO_CHANGE_ON_DESTROY,
    },
    { "auto-raise-delay",
      KEY_GENERAL_SCHEMA,
      META_PREF_AUTO_RAISE_DELAY,
      &auto_raise_delay,
      0, 10000, 0,
      /* @@@ Get rid of MAX_REASONABLE_AUTO_RAISE_DELAY */
    },
    { "cursor-size",
      KEY_MATE_MOUSE_SCHEMA,
      META_PREF_CURSOR_SIZE,
      &cursor_size,
      1, 128, 24,
    },
    { "icon-size",
      KEY_GENERAL_SCHEMA,
      META_PREF_ICON_SIZE,
      &icon_size,
      META_MIN_ICON_SIZE, META_MAX_ICON_SIZE, META_DEFAULT_ICON_SIZE,
    },
    { "alt-tab-max-columns",
      KEY_GENERAL_SCHEMA,
      META_PREF_ALT_TAB_MAX_COLUMNS,
      &alt_tab_max_columns,
      META_MIN_ALT_TAB_MAX_COLUMNS, 
      META_MAX_ALT_TAB_MAX_COLUMNS, 
      META_DEFAULT_ALT_TAB_MAX_COLUMNS,
    },
    { NULL, NULL, 0, NULL, 0, 0, 0, },
  };

static void
handle_preference_init_enum (void)
{
  MetaEnumPreference *cursor = preferences_enum;

  while (cursor->key!=NULL)
    {
      gint value;

      if (cursor->target==NULL)
        {
          ++cursor;
          continue;
        }

      value = g_settings_get_enum (SETTINGS (cursor->schema),
                                   cursor->key);
      *cursor->target = value;

      ++cursor;
    }
}

static void
handle_preference_init_bool (void)
{
  MetaBoolPreference *cursor = preferences_bool;

  while (cursor->key!=NULL)
    {
      if (cursor->target!=NULL)
        *cursor->target = g_settings_get_boolean (SETTINGS (cursor->schema), cursor->key);

      ++cursor;
    }

  maybe_give_disable_workarounds_warning ();
}

static void
handle_preference_init_string (void)
{
  MetaStringPreference *cursor = preferences_string;

  while (cursor->key!=NULL)
    {
      gchar *value;
      gboolean dummy = TRUE;

      /* the string "value" will be newly allocated */
      value = g_settings_get_string (SETTINGS (cursor->schema),
                                     cursor->key);

      if (cursor->handler)
        {
          if (cursor->target)
            meta_bug ("%s has both a target and a handler\n", cursor->key);

          cursor->handler (cursor->pref, value, &dummy);

          g_free (value);
        }
      else if (cursor->target)
        {
          if (*(cursor->target))
            g_free (*(cursor->target));

          *(cursor->target) = value;
        }

      ++cursor;
    }
}

static void
handle_preference_init_int (void)
{
  MetaIntPreference *cursor = preferences_int;


  while (cursor->key!=NULL)
    {
      gint value;

      value = g_settings_get_int (SETTINGS (cursor->schema),
                                  cursor->key);

      if (value < cursor->minimum || value > cursor->maximum)
        {
          /* FIXME: check if this can be avoided by GSettings */
          meta_warning (_("%d stored in GSettings key %s is out of range %d to %d\n"),
                        value, cursor->key,  cursor->minimum, cursor->maximum);
          /* Former behaviour for out-of-range values was:
           *   - number of workspaces was clamped;
           *   - auto raise delay was always reset to zero even if too high!;
           *   - cursor size was ignored.
           *
           * These seem to be meaningless variations.  If they did
           * have meaning we could have put them into MetaIntPreference.
           * The last of these is the closest to how we behave for
           * other types, so I think we should standardise on that.
           */
        }
      else if (cursor->target)
        *cursor->target = value;

      ++cursor;
    }
}

static gboolean
handle_preference_update_enum (const gchar *key, GSettings *settings)
{
  MetaEnumPreference *cursor = preferences_enum;
  gint old_value;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;

  /* We need to know whether the value changes, so
   * store the current value away.
   */

  old_value = * ((gint *) cursor->target);

  /* Now look it up... */
  *cursor->target = g_settings_get_enum (settings, key);

  /* Did it change?  If so, tell the listeners about it. */

  if (old_value != *((gint *) cursor->target))
    queue_changed (cursor->pref);

  return TRUE;
}

static gboolean
handle_preference_update_bool (const gchar *key, GSettings *settings)
{
  MetaBoolPreference *cursor = preferences_bool;
  gboolean old_value;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;

  if (cursor->target==NULL)
    /* No work for us to do. */
    return TRUE;

  /* We need to know whether the value changes, so
   * store the current value away.
   */

  old_value = * ((gboolean *) cursor->target);

  /* Now look it up... */

  *((gboolean *) cursor->target) = g_settings_get_boolean (settings, key);

  /* Did it change?  If so, tell the listeners about it. */

  if (old_value != *((gboolean *) cursor->target))
    queue_changed (cursor->pref);

  if (cursor->pref==META_PREF_DISABLE_WORKAROUNDS)
    maybe_give_disable_workarounds_warning ();

  return TRUE;
}

static gboolean
handle_preference_update_string (const gchar *key, GSettings *settings)
{
  MetaStringPreference *cursor = preferences_string;
  gchar *value;
  gboolean inform_listeners = TRUE;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;

  value = g_settings_get_string (settings, key);

  if (cursor->handler)
    cursor->handler (cursor->pref, value, &inform_listeners);
  else if (cursor->target)
    {
      if (*(cursor->target))
        g_free(*(cursor->target));

      if (value!=NULL)
        *(cursor->target) = g_strdup (value);
      else
        *(cursor->target) = NULL;

      inform_listeners =
        (value==NULL && *(cursor->target)==NULL) ||
        (value!=NULL && *(cursor->target)!=NULL &&
         strcmp (value, *(cursor->target))==0);
    }

  if (inform_listeners)
    queue_changed (cursor->pref);

  g_free (value);

  return TRUE;
}

static gboolean
handle_preference_update_int (const gchar *key, GSettings *settings)
{
  MetaIntPreference *cursor = preferences_int;
  gint value;

  while (cursor->key!=NULL && strcmp (key, cursor->key)!=0)
    ++cursor;

  if (cursor->key==NULL)
    /* Didn't recognise that key. */
    return FALSE;

  if (cursor->target==NULL)
    /* No work for us to do. */
    return TRUE;

  value = g_settings_get_int (settings, key);

  if (value < cursor->minimum || value > cursor->maximum)
    {
      /* FIXME! GSettings, instead of MateConf, has Minimum/Maximun in schema!
       * But some preferences depends on costants for minimum/maximum values */
      meta_warning (_("%d stored in GSettings key %s is out of range %d to %d\n"),
                    value, cursor->key,
                    cursor->minimum, cursor->maximum);
      return TRUE;
    }

  /* Did it change?  If so, tell the listeners about it. */

  if (*cursor->target != value)
    {
      *cursor->target = value;
      queue_changed (cursor->pref);
    }

  return TRUE;

}


/****************************************************************************/
/* Listeners.                                                               */
/****************************************************************************/

void
meta_prefs_add_listener (MetaPrefsChangedFunc func,
                         gpointer             data)
{
  MetaPrefsListener *l;

  l = g_new (MetaPrefsListener, 1);
  l->func = func;
  l->data = data;

  listeners = g_list_prepend (listeners, l);
}

void
meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                            gpointer             data)
{
  GList *tmp;

  tmp = listeners;
  while (tmp != NULL)
    {
      MetaPrefsListener *l = tmp->data;

      if (l->func == func &&
          l->data == data)
        {
          g_free (l);
          listeners = g_list_delete_link (listeners, tmp);

          return;
        }

      tmp = tmp->next;
    }

  meta_bug ("Did not find listener to remove\n");
}

static void
emit_changed (MetaPreference pref)
{
  GList *tmp;
  GList *copy;

  meta_topic (META_DEBUG_PREFS, "Notifying listeners that pref %s changed\n",
              meta_preference_to_string (pref));

  copy = g_list_copy (listeners);

  tmp = copy;

  while (tmp != NULL)
    {
      MetaPrefsListener *l = tmp->data;

      (* l->func) (pref, l->data);

      tmp = tmp->next;
    }

  g_list_free (copy);
}

static gboolean
changed_idle_handler (gpointer data)
{
  GList *tmp;
  GList *copy;

  changed_idle = 0;

  copy = g_list_copy (changes); /* reentrancy paranoia */

  g_list_free (changes);
  changes = NULL;

  tmp = copy;
  while (tmp != NULL)
    {
      MetaPreference pref = GPOINTER_TO_INT (tmp->data);

      emit_changed (pref);

      tmp = tmp->next;
    }

  g_list_free (copy);

  return FALSE;
}

static void
queue_changed (MetaPreference pref)
{
  meta_topic (META_DEBUG_PREFS, "Queueing change of pref %s\n",
              meta_preference_to_string (pref));

  if (g_list_find (changes, GINT_TO_POINTER (pref)) == NULL)
    changes = g_list_prepend (changes, GINT_TO_POINTER (pref));
  else
    meta_topic (META_DEBUG_PREFS, "Change of pref %s was already pending\n",
                meta_preference_to_string (pref));

  /* add idle at priority below the GSettings notify idle */
  /* FIXME is this needed for GSettings too? */
  if (changed_idle == 0)
    changed_idle = g_idle_add_full (META_PRIORITY_PREFS_NOTIFY,
                                    changed_idle_handler, NULL, NULL);
}


/****************************************************************************/
/* Initialisation.                                                          */
/****************************************************************************/

void
meta_prefs_init (void)
{
  if (settings_general != NULL)
    return;

  /* returns references which we hold forever */
  settings_general = g_settings_new (KEY_GENERAL_SCHEMA);
  settings_command = g_settings_new (KEY_COMMAND_SCHEMA);
  settings_screen_bindings = g_settings_new (KEY_SCREEN_BINDINGS_SCHEMA);
  settings_window_bindings = g_settings_new (KEY_WINDOW_BINDINGS_SCHEMA);
  settings_workspace_names = g_settings_new (KEY_WORKSPACE_NAME_SCHEMA);
  settings_mate_interface = g_settings_new (KEY_MATE_INTERFACE_SCHEMA);
  settings_mate_terminal = g_settings_new (KEY_MATE_TERMINAL_SCHEMA);
  settings_mate_mouse = g_settings_new (KEY_MATE_MOUSE_SCHEMA);

  settings_schemas = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
  g_hash_table_insert (settings_schemas, KEY_GENERAL_SCHEMA, settings_general);
  g_hash_table_insert (settings_schemas, KEY_COMMAND_SCHEMA, settings_command);
  g_hash_table_insert (settings_schemas, KEY_SCREEN_BINDINGS_SCHEMA, settings_screen_bindings);
  g_hash_table_insert (settings_schemas, KEY_WINDOW_BINDINGS_SCHEMA, settings_window_bindings);
  g_hash_table_insert (settings_schemas, KEY_WORKSPACE_NAME_SCHEMA, settings_workspace_names);
  g_hash_table_insert (settings_schemas, KEY_MATE_INTERFACE_SCHEMA, settings_mate_interface);
  g_hash_table_insert (settings_schemas, KEY_MATE_TERMINAL_SCHEMA, settings_mate_terminal);
  g_hash_table_insert (settings_schemas, KEY_MATE_MOUSE_SCHEMA, settings_mate_mouse);

  g_signal_connect (settings_general, "changed", G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_command, "changed", G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_screen_bindings, "changed", G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_window_bindings, "changed", G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_workspace_names, "changed", G_CALLBACK (change_notify), NULL);

  g_signal_connect (settings_mate_interface, "changed::" KEY_MATE_INTERFACE_ACCESSIBILITY, G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_mate_interface, "changed::" KEY_MATE_INTERFACE_ENABLE_ANIMATIONS, G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_mate_terminal, "changed::" KEY_MATE_TERMINAL_COMMAND, G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_mate_mouse, "changed::" KEY_MATE_MOUSE_CURSOR_THEME, G_CALLBACK (change_notify), NULL);
  g_signal_connect (settings_mate_mouse, "changed::" KEY_MATE_MOUSE_CURSOR_SIZE, G_CALLBACK (change_notify), NULL);

  /* Pick up initial values. */

  handle_preference_init_enum ();
  handle_preference_init_bool ();
  handle_preference_init_string ();
  handle_preference_init_int ();

  init_screen_bindings ();
  init_window_bindings ();
  init_commands ();
  init_workspace_names ();
}

/****************************************************************************/
/* Updates.                                                                 */
/****************************************************************************/

gboolean (*preference_update_handler[]) (const gchar*, GSettings*) = {
  handle_preference_update_enum,
  handle_preference_update_bool,
  handle_preference_update_string,
  handle_preference_update_int,
  NULL
};

static void
change_notify (GSettings *settings,
               gchar *key,
               gpointer user_data)
{
  gint i=0;

  /* First, search for a handler that might know what to do. */

  /* FIXME: When this is all working, since the first item in every
   * array is the gchar* of the key, there's no reason we can't
   * find the correct record for that key here and save code duplication.
   */

  while (preference_update_handler[i]!=NULL)
    {
      if (preference_update_handler[i] (key, settings))
        return; /* Get rid of this eventually */

      i++;
    }

  gchar *schema_name = NULL;
  g_object_get (settings, "schema-id", &schema_name, NULL);

  if (g_strcmp0 (schema_name, KEY_WINDOW_BINDINGS_SCHEMA) == 0 ||
      g_strcmp0 (schema_name, KEY_SCREEN_BINDINGS_SCHEMA) == 0)
    {
      gchar *str;
      str = g_settings_get_string (settings, key);

      if (update_key_binding (key, str))
        queue_changed (META_PREF_KEYBINDINGS);

      g_free(str);
    }
  else if (g_strcmp0 (schema_name, KEY_COMMAND_SCHEMA) == 0)
    {
      gchar *str;
      str = g_settings_get_string (settings, key);

      if (update_command (key, str))
        queue_changed (META_PREF_COMMANDS);

      g_free(str);
    }
  else if (g_strcmp0 (schema_name, KEY_WORKSPACE_NAME_SCHEMA) == 0)
    {
      gchar *str;
        str = g_settings_get_string (settings, key);

      if (update_workspace_name (key, str))
        queue_changed (META_PREF_WORKSPACE_NAMES);

      g_free(str);
    }
  else
    {
      /* Is this possible with GSettings? I dont think so! */
      meta_topic (META_DEBUG_PREFS, "Key %s doesn't mean anything to Marco\n",
                  key);
    }
  g_free (schema_name);
}

#if 0
static void
cleanup_error (GError **error)
{
  if (*error)
    {
      meta_warning ("%s\n", (*error)->message);

      g_error_free (*error);
      *error = NULL;
    }
}
#endif

/**
 * Special case: give a warning the first time disable_workarounds
 * is turned on.
 */
static void
maybe_give_disable_workarounds_warning (void)
{
  static gboolean first_disable = TRUE;

  if (first_disable && disable_workarounds)
    {
      first_disable = FALSE;

      meta_warning (_("Workarounds for broken applications disabled. "
                      "Some applications may not behave properly.\n"));
    }
}

MetaVirtualModifier
meta_prefs_get_mouse_button_mods  (void)
{
  return mouse_button_mods;
}

MetaFocusMode
meta_prefs_get_focus_mode (void)
{
  return focus_mode;
}

MetaFocusNewWindows
meta_prefs_get_focus_new_windows (void)
{
  return focus_new_windows;
}

gboolean
meta_prefs_get_attach_modal_dialogs (void)
{
  return attach_modal_dialogs;
}

gboolean
meta_prefs_get_raise_on_click (void)
{
  /* Force raise_on_click on for click-to-focus, as requested by Havoc
   * in #326156.
   */
  return raise_on_click || focus_mode == META_FOCUS_MODE_CLICK;
}

const char*
meta_prefs_get_theme (void)
{
  return current_theme;
}

const char*
meta_prefs_get_cursor_theme (void)
{
  return cursor_theme;
}

int
meta_prefs_get_cursor_size (void)
{
  GdkWindow *window = gdk_get_default_root_window ();
  gint scale = gdk_window_get_scale_factor (window);

  return cursor_size * scale;
}

int
meta_prefs_get_icon_size (void)
{
  return icon_size;
}

int
meta_prefs_get_alt_tab_max_columns (void)
{
  return alt_tab_max_columns;
}

gboolean
meta_prefs_is_in_skip_list (char *class)
{
  GList *item;
    
  for (item = show_desktop_skip_list; item; item = item->next)
    {
      if (!g_ascii_strcasecmp (class, item->data))
        return TRUE;
    }
  return FALSE;
}

/****************************************************************************/
/* Handlers for string preferences.                                         */
/****************************************************************************/

static void
titlebar_handler (MetaPreference pref,
                  const gchar    *string_value,
                  gboolean       *inform_listeners)
{
  PangoFontDescription *new_desc = NULL;

  if (string_value)
    new_desc = pango_font_description_from_string (string_value);

  if (new_desc == NULL)
    {
      meta_warning (_("Could not parse font description "
                      "\"%s\" from GSettings key %s\n"),
                    string_value ? string_value : "(null)",
                    KEY_GENERAL_TITLEBAR_FONT);

      *inform_listeners = FALSE;

      return;
    }

  /* Is the new description the same as the old? */

  if (titlebar_font &&
      pango_font_description_equal (new_desc, titlebar_font))
    {
      pango_font_description_free (new_desc);
      *inform_listeners = FALSE;
      return;
    }

  /* No, so free the old one and put ours in instead. */

  if (titlebar_font)
    pango_font_description_free (titlebar_font);

  titlebar_font = new_desc;

}

static void
theme_name_handler (MetaPreference pref,
                    const gchar *string_value,
                    gboolean *inform_listeners)
{
  g_free (current_theme);

  /* Fallback crackrock */
  if (string_value == NULL)
    current_theme = g_strdup ("ClearlooksRe");
  else
    current_theme = g_strdup (string_value);
}

static void
mouse_button_mods_handler (MetaPreference pref,
                           const gchar *string_value,
                           gboolean *inform_listeners)
{
  MetaVirtualModifier mods;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Mouse button modifier has new GSettings value \"%s\"\n",
              string_value);
  if (string_value && meta_ui_parse_modifier (string_value, &mods))
    {
      mouse_button_mods = mods;
    }
  else
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Failed to parse new GSettings value\n");

      meta_warning (_("\"%s\" found in configuration database is "
                      "not a valid value for mouse button modifier\n"),
                    string_value);

      *inform_listeners = FALSE;
    }
}

static void
show_desktop_skip_list_handler (MetaPreference pref,
                                const gchar *string_value,
                                gboolean *inform_listeners)
{
  gchar **tokens;
  gchar **tok;
  GList *item;

  if (show_desktop_skip_list)
    {
      for (item = show_desktop_skip_list; item; item = item->next)
        g_free (item->data);
      g_list_free (show_desktop_skip_list);
      show_desktop_skip_list = NULL;
    }

  if (!(tokens = g_strsplit (string_value, ",", -1)))
    return;
  for (tok = tokens; tok && *tok; tok++)
    {
      gchar *stripped = g_strstrip (g_strdup (*tok));
      show_desktop_skip_list = g_list_prepend (show_desktop_skip_list, stripped);
    }
  g_strfreev (tokens);
}

static gboolean
button_layout_equal (const MetaButtonLayout *a,
                     const MetaButtonLayout *b)
{
  int i;

  i = 0;
  while (i < MAX_BUTTONS_PER_CORNER)
    {
      if (a->left_buttons[i] != b->left_buttons[i])
        return FALSE;
      if (a->right_buttons[i] != b->right_buttons[i])
        return FALSE;
      if (a->left_buttons_has_spacer[i] != b->left_buttons_has_spacer[i])
        return FALSE;
      if (a->right_buttons_has_spacer[i] != b->right_buttons_has_spacer[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}

static MetaButtonFunction
button_function_from_string (const char *str)
{
  /* FIXME: g_settings_get_enum is the obvious way to do this */

  if (strcmp (str, "menu") == 0)
    return META_BUTTON_FUNCTION_MENU;
  else if (strcmp (str, "appmenu") == 0)
    return META_BUTTON_FUNCTION_APPMENU;
  else if (strcmp (str, "minimize") == 0)
    return META_BUTTON_FUNCTION_MINIMIZE;
  else if (strcmp (str, "maximize") == 0)
    return META_BUTTON_FUNCTION_MAXIMIZE;
  else if (strcmp (str, "close") == 0)
    return META_BUTTON_FUNCTION_CLOSE;
  else if (strcmp (str, "shade") == 0)
    return META_BUTTON_FUNCTION_SHADE;
  else if (strcmp (str, "above") == 0)
    return META_BUTTON_FUNCTION_ABOVE;
  else if (strcmp (str, "stick") == 0)
    return META_BUTTON_FUNCTION_STICK;
  else
    /* don't know; give up */
    return META_BUTTON_FUNCTION_LAST;
}

static MetaButtonFunction
button_opposite_function (MetaButtonFunction ofwhat)
{
  switch (ofwhat)
    {
    case META_BUTTON_FUNCTION_SHADE:
      return META_BUTTON_FUNCTION_UNSHADE;
    case META_BUTTON_FUNCTION_UNSHADE:
      return META_BUTTON_FUNCTION_SHADE;

    case META_BUTTON_FUNCTION_ABOVE:
      return META_BUTTON_FUNCTION_UNABOVE;
    case META_BUTTON_FUNCTION_UNABOVE:
      return META_BUTTON_FUNCTION_ABOVE;

    case META_BUTTON_FUNCTION_STICK:
      return META_BUTTON_FUNCTION_UNSTICK;
    case META_BUTTON_FUNCTION_UNSTICK:
      return META_BUTTON_FUNCTION_STICK;

    default:
      return META_BUTTON_FUNCTION_LAST;
    }
}

static void
button_layout_handler (MetaPreference pref,
                         const gchar *string_value,
                         gboolean *inform_listeners)
{
  MetaButtonLayout new_layout;
  char **sides = NULL;
  int i;

  /* We need to ignore unknown button functions, for
   * compat with future versions
   */

  if (string_value)
    sides = g_strsplit (string_value, ":", 2);

  if (sides != NULL && sides[0] != NULL)
    {
      char **buttons;
      int b;
      gboolean used[META_BUTTON_FUNCTION_LAST];

      i = 0;
      while (i < META_BUTTON_FUNCTION_LAST)
        {
          used[i] = FALSE;
          new_layout.left_buttons_has_spacer[i] = FALSE;
          ++i;
        }

      buttons = g_strsplit (sides[0], ",", -1);
      i = 0;
      b = 0;
      while (buttons[b] != NULL)
        {
          MetaButtonFunction f = button_function_from_string (buttons[b]);
          if (i > 0 && strcmp("spacer", buttons[b]) == 0)
            {
              new_layout.left_buttons_has_spacer[i-1] = TRUE;
              f = button_opposite_function (f);

              if (f != META_BUTTON_FUNCTION_LAST)
                {
                  new_layout.left_buttons_has_spacer[i-2] = TRUE;
                }
            }
          else
            {
              if (f != META_BUTTON_FUNCTION_LAST && !used[f])
                {
                  new_layout.left_buttons[i] = f;
                  used[f] = TRUE;
                  ++i;

                  f = button_opposite_function (f);

                  if (f != META_BUTTON_FUNCTION_LAST)
                      new_layout.left_buttons[i++] = f;

                }
              else
                {
                  meta_topic (META_DEBUG_PREFS, "Ignoring unknown or already-used button name \"%s\"\n",
                              buttons[b]);
                }
            }

          ++b;
        }

      new_layout.left_buttons[i] = META_BUTTON_FUNCTION_LAST;
      new_layout.left_buttons_has_spacer[i] = FALSE;

      g_strfreev (buttons);
    }

  if (sides != NULL && sides[0] != NULL && sides[1] != NULL)
    {
      char **buttons;
      int b;
      gboolean used[META_BUTTON_FUNCTION_LAST];

      i = 0;
      while (i < META_BUTTON_FUNCTION_LAST)
        {
          used[i] = FALSE;
          new_layout.right_buttons_has_spacer[i] = FALSE;
          ++i;
        }

      buttons = g_strsplit (sides[1], ",", -1);
      i = 0;
      b = 0;
      while (buttons[b] != NULL)
        {
          MetaButtonFunction f = button_function_from_string (buttons[b]);
          if (i > 0 && strcmp("spacer", buttons[b]) == 0)
            {
              new_layout.right_buttons_has_spacer[i-1] = TRUE;
              f = button_opposite_function (f);
              if (f != META_BUTTON_FUNCTION_LAST)
                {
                  new_layout.right_buttons_has_spacer[i-2] = TRUE;
                }
            }
          else
            {
              if (f != META_BUTTON_FUNCTION_LAST && !used[f])
                {
                  new_layout.right_buttons[i] = f;
                  used[f] = TRUE;
                  ++i;

                  f = button_opposite_function (f);

                  if (f != META_BUTTON_FUNCTION_LAST)
                      new_layout.right_buttons[i++] = f;

                }
              else
                {
                  meta_topic (META_DEBUG_PREFS, "Ignoring unknown or already-used button name \"%s\"\n",
                              buttons[b]);
                }
            }

          ++b;
        }

      new_layout.right_buttons[i] = META_BUTTON_FUNCTION_LAST;
      new_layout.right_buttons_has_spacer[i] = FALSE;

      g_strfreev (buttons);
    }

  g_strfreev (sides);

  /* Invert the button layout for RTL languages */
  if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
  {
    MetaButtonLayout rtl_layout;
    int j;

    for (i = 0; new_layout.left_buttons[i] != META_BUTTON_FUNCTION_LAST; i++);
    for (j = 0; j < i; j++)
      {
        rtl_layout.right_buttons[j] = new_layout.left_buttons[i - j - 1];
        if (j == 0)
          rtl_layout.right_buttons_has_spacer[i - 1] = new_layout.left_buttons_has_spacer[i - j - 1];
        else
          rtl_layout.right_buttons_has_spacer[j - 1] = new_layout.left_buttons_has_spacer[i - j - 1];
      }
    rtl_layout.right_buttons[j] = META_BUTTON_FUNCTION_LAST;
    rtl_layout.right_buttons_has_spacer[j] = FALSE;

    for (i = 0; new_layout.right_buttons[i] != META_BUTTON_FUNCTION_LAST; i++);
    for (j = 0; j < i; j++)
      {
        rtl_layout.left_buttons[j] = new_layout.right_buttons[i - j - 1];
        if (j == 0)
          rtl_layout.left_buttons_has_spacer[i - 1] = new_layout.right_buttons_has_spacer[i - j - 1];
        else
          rtl_layout.left_buttons_has_spacer[j - 1] = new_layout.right_buttons_has_spacer[i - j - 1];
      }
    rtl_layout.left_buttons[j] = META_BUTTON_FUNCTION_LAST;
    rtl_layout.left_buttons_has_spacer[j] = FALSE;

    new_layout = rtl_layout;
  }

  if (button_layout_equal (&button_layout, &new_layout))
    {
      /* Same as before, so duck out */
      *inform_listeners = FALSE;
    }
  else
    {
      button_layout = new_layout;
    }
}

const PangoFontDescription*
meta_prefs_get_titlebar_font (void)
{
  if (use_system_font)
    return NULL;
  else
    return titlebar_font;
}

int
meta_prefs_get_num_workspaces (void)
{
  return num_workspaces;
}

MetaWrapStyle
meta_prefs_get_wrap_style (void)
{
  return wrap_style;
}

gboolean
meta_prefs_get_application_based (void)
{
  return FALSE; /* For now, we never want this to do anything */

  return application_based;
}

gboolean
meta_prefs_get_disable_workarounds (void)
{
  return disable_workarounds;
}

#define MAX_REASONABLE_AUTO_RAISE_DELAY 10000

#ifdef WITH_VERBOSE_MODE
const char*
meta_preference_to_string (MetaPreference pref)
{
  /* FIXME: another case for g_settings_get_enum */
  switch (pref)
    {
    case META_PREF_MOUSE_BUTTON_MODS:
      return "MOUSE_BUTTON_MODS";

    case META_PREF_FOCUS_MODE:
      return "FOCUS_MODE";

    case META_PREF_FOCUS_NEW_WINDOWS:
      return "FOCUS_NEW_WINDOWS";

    case META_PREF_ATTACH_MODAL_DIALOGS:
      return "ATTACH_MODAL_DIALOGS";

    case META_PREF_RAISE_ON_CLICK:
      return "RAISE_ON_CLICK";

    case META_PREF_THEME:
      return "THEME";

    case META_PREF_TITLEBAR_FONT:
      return "TITLEBAR_FONT";

    case META_PREF_NUM_WORKSPACES:
      return "NUM_WORKSPACES";

    case META_PREF_WRAP_STYLE:
      return "WRAP_STYLE";

    case META_PREF_APPLICATION_BASED:
      return "APPLICATION_BASED";

    case META_PREF_KEYBINDINGS:
      return "KEYBINDINGS";

    case META_PREF_DISABLE_WORKAROUNDS:
      return "DISABLE_WORKAROUNDS";

    case META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR:
      return "ACTION_DOUBLE_CLICK_TITLEBAR";

    case META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR:
      return "ACTION_MIDDLE_CLICK_TITLEBAR";

    case META_PREF_ACTION_RIGHT_CLICK_TITLEBAR:
      return "ACTION_RIGHT_CLICK_TITLEBAR";

    case META_PREF_AUTO_RAISE:
      return "AUTO_RAISE";

    case META_PREF_AUTO_RAISE_DELAY:
      return "AUTO_RAISE_DELAY";

    case META_PREF_COMMANDS:
      return "COMMANDS";

    case META_PREF_TERMINAL_COMMAND:
      return "TERMINAL_COMMAND";

    case META_PREF_BUTTON_LAYOUT:
      return "BUTTON_LAYOUT";

    case META_PREF_WORKSPACE_NAMES:
      return "WORKSPACE_NAMES";

    case META_PREF_VISUAL_BELL:
      return "VISUAL_BELL";

    case META_PREF_AUDIBLE_BELL:
      return "AUDIBLE_BELL";

    case META_PREF_VISUAL_BELL_TYPE:
      return "VISUAL_BELL_TYPE";

    case META_PREF_REDUCED_RESOURCES:
      return "REDUCED_RESOURCES";

    case META_PREF_MATE_ACCESSIBILITY:
      return "MATE_ACCESSIBILTY";

    case META_PREF_MATE_ANIMATIONS:
      return "MATE_ANIMATIONS";

    case META_PREF_CURSOR_THEME:
      return "CURSOR_THEME";

    case META_PREF_CURSOR_SIZE:
      return "CURSOR_SIZE";

    case META_PREF_ICON_SIZE:
      return "ICON_SIZE";

    case META_PREF_ALT_TAB_MAX_COLUMNS:
      return "ALT_TAB_MAX_COLUMNS";

    case META_PREF_COMPOSITING_MANAGER:
      return "COMPOSITING_MANAGER";

    case META_PREF_COMPOSITING_FAST_ALT_TAB:
      return "COMPOSITING_FAST_ALT_TAB";

    case META_PREF_CENTER_NEW_WINDOWS:
      return "CENTER_NEW_WINDOWS";

    case META_PREF_RESIZE_WITH_RIGHT_BUTTON:
      return "RESIZE_WITH_RIGHT_BUTTON";

    case META_PREF_SHOW_TAB_BORDER:
      return "SHOW_TAB_BORDER";

    case META_PREF_FORCE_FULLSCREEN:
      return "FORCE_FULLSCREEN";

    case META_PREF_ALLOW_TILING:
      return "ALLOW_TILING";

    case META_PREF_ALLOW_TOP_TILING:
      return "ALLOW_TOP_TILING";

    case META_PREF_PLACEMENT_MODE:
      return "PLACEMENT_MODE";

    case META_PREF_SHOW_DESKTOP_SKIP_LIST:
      return "SHOW_DESKTOP_SKIP_LIST";
    }

  return "(unknown)";
}
#endif /* WITH_VERBOSE_MODE */

void
meta_prefs_set_num_workspaces (int n_workspaces)
{
  if (n_workspaces < 1)
    n_workspaces = 1;
  if (n_workspaces > MAX_REASONABLE_WORKSPACES)
    n_workspaces = MAX_REASONABLE_WORKSPACES;

  g_settings_set_int (settings_general,
                      KEY_GENERAL_NUM_WORKSPACES,
                      n_workspaces);

}

#define keybind(name, handler, param, flags) \
  { #name, NULL, !!(flags & BINDING_REVERSES), !!(flags & BINDING_PER_WINDOW) },
static MetaKeyPref key_bindings[] = {
#include "all-keybindings.h"
  { NULL, NULL, FALSE }
};
#undef keybind

static void
init_bindings (GSettings *settings)
{
  GSettingsSchema *schema;
  gchar **list = NULL;
  gchar *str_val = NULL;

  g_object_get (settings, "settings-schema", &schema, NULL);
  list = g_settings_schema_list_keys (schema);
  g_settings_schema_unref (schema);

  while (*list != NULL)
    {
      str_val = g_settings_get_string (settings, *list);
      update_key_binding (*list, str_val);
      list++;
    }

  g_free (str_val);
}

static void
init_screen_bindings (void)
{
  init_bindings (settings_screen_bindings);
}

static void
init_window_bindings (void)
{
  init_bindings (settings_window_bindings);
}

static void
init_commands (void)
{
  GSettingsSchema *schema;
  gchar **list = NULL;
  gchar *str_val = NULL;

  g_object_get (settings_command, "settings-schema", &schema, NULL);
  list = g_settings_schema_list_keys (schema);
  g_settings_schema_unref (schema);

  while (*list != NULL)
    {
      str_val = g_settings_get_string (settings_command, *list);
      update_command (*list, str_val);
      list++;
    }

  g_free (str_val);
}

static void
init_workspace_names (void)
{
  GSettingsSchema *schema;
  gchar **list = NULL;
  gchar *str_val = NULL;

  g_object_get (settings_workspace_names, "settings-schema", &schema, NULL);
  list = g_settings_schema_list_keys (schema);
  g_settings_schema_unref (schema);

  while (*list != NULL)
    {
      str_val = g_settings_get_string (settings_workspace_names, *list);
      update_workspace_name (*list, str_val);
      list++;
    }

  g_free (str_val);
}

static gboolean
update_binding (MetaKeyPref *binding,
                gchar  *value)
{
  unsigned int keysym;
  unsigned int keycode;
  MetaVirtualModifier mods;
  MetaKeyCombo *combo;
  gboolean changed;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Binding \"%s\" has new GSettings value \"%s\"\n",
              binding->name, value ? value : "none");

  keysym = 0;
  keycode = 0;
  mods = 0;
  if (value)
    {
      if (!meta_ui_parse_accelerator (value, &keysym, &keycode, &mods))
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Failed to parse new GSettings value\n");
          meta_warning (_("\"%s\" found in configuration database is not a valid value for keybinding \"%s\"\n"),
                        value, binding->name);

          return FALSE;
        }
    }

  /* If there isn't already a first element, make one. */
  if (!binding->bindings)
    {
      MetaKeyCombo *blank = g_malloc0 (sizeof (MetaKeyCombo));
      binding->bindings = g_slist_alloc();
      binding->bindings->data = blank;
    }

   combo = binding->bindings->data;

   /* Bug 329676: Bindings which can be shifted must not have no modifiers,
    * nor only SHIFT as a modifier.
    */

  if (binding->add_shift &&
      0 != keysym &&
      (META_VIRTUAL_SHIFT_MASK == mods || 0 == mods))
    {
      gchar *old_setting;

      meta_warning ("Cannot bind \"%s\" to %s: it needs a modifier "
                    "such as Ctrl or Alt.\n",
                    binding->name,
                    value);

      old_setting = meta_ui_accelerator_name (combo->keysym,
                                              combo->modifiers);

      if (!strcmp(old_setting, value))
        {
          /* We were about to set it to the same value
           * that it had originally! This must be caused
           * by getting an invalid string back from
           * meta_ui_accelerator_name. Bail out now
           * so we don't get into an infinite loop.
           */
           g_free (old_setting);
           return TRUE;
        }

      meta_warning ("Reverting \"%s\" to %s.\n",
                    binding->name,
                    old_setting);

      /* FIXME: add_shift is currently screen_bindings only, but
       * there's no really good reason it should always be.
       * So we shouldn't blindly add KEY_SCREEN_BINDINGS_PREFIX
       * onto here.
       */
      g_settings_set_string(settings_screen_bindings,
                            binding->name,
                            old_setting);

      g_free (old_setting);

      /* The call to g_settings_set_string() will cause this function
       * to be called again with the new value, so there's no need to
       * carry on.
       */
      return TRUE;
    }

  changed = FALSE;
  if (keysym != combo->keysym ||
      keycode != combo->keycode ||
      mods != combo->modifiers)
    {
      changed = TRUE;

      combo->keysym = keysym;
      combo->keycode = keycode;
      combo->modifiers = mods;

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "New keybinding for \"%s\" is keysym = 0x%x keycode = 0x%x mods = 0x%x\n",
                  binding->name, combo->keysym, combo->keycode,
                  combo->modifiers);
    }
  else
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Keybinding for \"%s\" is unchanged\n", binding->name);
    }

  return changed;
}

static const gchar*
relative_key (const gchar* key)
{
  const gchar* end;

  end = strrchr (key, '/');

  ++end;

  return end;
}

/* Return value is TRUE if a preference changed and we need to
 * notify
 */
static gboolean
find_and_update_binding (MetaKeyPref *bindings,
                         const char  *name,
                         gchar  *value)
{
  const char *key;
  int i;

  if (*name == '/')
    key = relative_key (name);
  else
    key = name;

  i = 0;
  while (bindings[i].name &&
         strcmp (key, bindings[i].name) != 0)
    ++i;

  if (bindings[i].name)
    return update_binding (&bindings[i], value);
  else
    return FALSE;
}

static gboolean
update_key_binding (const char *name,
                    gchar *value)
{
  return find_and_update_binding (key_bindings, name, value);
}

static gboolean
update_command (const char  *name,
                const char  *value)
{
  char *p;
  int i;

  p = strrchr (name, '-');
  if (p == NULL)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Command %s has no dash?\n", name);
      return FALSE;
    }

  ++p;

  if (g_ascii_isdigit (*p))
    {
      i = atoi (p);
      i -= 1; /* count from 0 not 1 */
    }
  else
    {
      if (strcmp (name, "command-screenshot") == 0)
        {
          i = SCREENSHOT_COMMAND_IDX;
        }
      else if (strcmp (name, "command-window-screenshot") == 0)
        {
          i = WIN_SCREENSHOT_COMMAND_IDX;
        }
      else
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Command %s doesn't end in number?\n", name);
          return FALSE;
        }
    }

  if (i >= MAX_COMMANDS)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Command %d is too highly numbered, ignoring\n", i);
      return FALSE;
    }

  if ((commands[i] == NULL && value == NULL) ||
      (commands[i] && value && strcmp (commands[i], value) == 0))
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Command %d is unchanged\n", i);
      return FALSE;
    }

  g_free (commands[i]);
  commands[i] = g_strdup (value);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Updated command %d to \"%s\"\n",
              i, commands[i] ? commands[i] : "none");

  return TRUE;
}

const char*
meta_prefs_get_command (int i)
{
  g_return_val_if_fail (i >= 0 && i < MAX_COMMANDS, NULL);

  return commands[i];
}

char*
meta_prefs_get_settings_key_for_command (int i)
{
  char *key;

  switch (i)
    {
    case SCREENSHOT_COMMAND_IDX:
      key = g_strdup (KEY_COMMAND_PREFIX "screenshot");
      break;
    case WIN_SCREENSHOT_COMMAND_IDX:
      key = g_strdup (KEY_COMMAND_PREFIX "window-screenshot");
      break;
    default:
      key = g_strdup_printf (KEY_COMMAND_PREFIX"%d", i + 1);
      break;
    }

  return key;
}

const char*
meta_prefs_get_terminal_command (void)
{
  return terminal_command;
}

const char*
meta_prefs_get_settings_key_for_terminal_command (void)
{
  return KEY_MATE_TERMINAL_COMMAND;
}

static gboolean
update_workspace_name (const char  *name,
                       const char  *value)
{
  char *p;
  int i;

  p = strrchr (name, '-');
  if (p == NULL)
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %s has no dash?\n", name);
      return FALSE;
    }

  ++p;

  if (!g_ascii_isdigit (*p))
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %s doesn't end in number?\n", name);
      return FALSE;
    }

  i = atoi (p);
  i -= 1; /* count from 0 not 1 */

  if (i >= MAX_REASONABLE_WORKSPACES)
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %d is too highly numbered, ignoring\n", i);
      return FALSE;
    }

  if (workspace_names[i] && value && strcmp (workspace_names[i], value) == 0)
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace name %d is unchanged\n", i);
      return FALSE;
    }

  /* This is a bad hack. We have to treat empty string as
   * "unset" because the root window property can't contain
   * null. So it gets empty string instead and we don't want
   * that to result in setting the empty string as a value that
   * overrides "unset".
   */
  if (value != NULL && *value != '\0')
    {
      g_free (workspace_names[i]);
      workspace_names[i] = g_strdup (value);
    }
  else
    {
      /* use a default name */
      char *d;

      d = g_strdup_printf (_("Workspace %d"), i + 1);
      if (workspace_names[i] && strcmp (workspace_names[i], d) == 0)
        {
          g_free (d);
          return FALSE;
        }
      else
        {
          g_free (workspace_names[i]);
          workspace_names[i] = d;
        }
    }

  meta_topic (META_DEBUG_PREFS,
              "Updated workspace name %d to \"%s\"\n",
              i, workspace_names[i] ? workspace_names[i] : "none");

  return TRUE;
}

const char*
meta_prefs_get_workspace_name (int i)
{
  g_return_val_if_fail (i >= 0 && i < MAX_REASONABLE_WORKSPACES, NULL);

  g_assert (workspace_names[i] != NULL);

  meta_topic (META_DEBUG_PREFS,
              "Getting workspace name for %d: \"%s\"\n",
              i, workspace_names[i]);

  return workspace_names[i];
}

void
meta_prefs_change_workspace_name (int         i,
                                  const char *name)
{
  char *key;

  g_return_if_fail (i >= 0 && i < MAX_REASONABLE_WORKSPACES);

  meta_topic (META_DEBUG_PREFS,
              "Changing name of workspace %d to %s\n",
              i, name ? name : "none");

  if (name && *name == '\0')
    name = NULL;

  if ((name == NULL && workspace_names[i] == NULL) ||
      (name && workspace_names[i] && strcmp (name, workspace_names[i]) == 0))
    {
      meta_topic (META_DEBUG_PREFS,
                  "Workspace %d already has name %s\n",
                  i, name ? name : "none");
      return;
    }

  key = settings_key_for_workspace_name (i);

  if (name != NULL)
    g_settings_set_string (settings_workspace_names,
                          key,
                          name);
  else
    g_settings_set_string (settings_workspace_names,
                          key,
                          "");

  g_free (key);
}

static char*
settings_key_for_workspace_name (int i)
{
  char *key;

  key = g_strdup_printf (KEY_WORKSPACE_NAME_PREFIX"%d", i + 1);

  return key;
}

void
meta_prefs_get_button_layout (MetaButtonLayout *button_layout_p)
{
  *button_layout_p = button_layout;
}

gboolean
meta_prefs_get_visual_bell (void)
{
  return provide_visual_bell;
}

gboolean
meta_prefs_bell_is_audible (void)
{
  return bell_is_audible;
}

MetaVisualBellType
meta_prefs_get_visual_bell_type (void)
{
  return visual_bell_type;
}

void
meta_prefs_get_key_bindings (const MetaKeyPref **bindings,
                                int                *n_bindings)
{

  *bindings = key_bindings;
  *n_bindings = (int) G_N_ELEMENTS (key_bindings) - 1;
}

MetaActionTitlebar
meta_prefs_get_action_double_click_titlebar (void)
{
  return action_double_click_titlebar;
}

MetaActionTitlebar
meta_prefs_get_action_middle_click_titlebar (void)
{
  return action_middle_click_titlebar;
}

MetaActionTitlebar
meta_prefs_get_action_right_click_titlebar (void)
{
  return action_right_click_titlebar;
}

gboolean
meta_prefs_get_auto_raise (void)
{
  return auto_raise;
}

int
meta_prefs_get_auto_raise_delay (void)
{
  return auto_raise_delay;
}

gboolean
meta_prefs_get_reduced_resources (void)
{
  return reduced_resources;
}

gboolean
meta_prefs_get_mate_accessibility ()
{
  return mate_accessibility;
}

gboolean
meta_prefs_get_mate_animations ()
{
  return mate_animations;
}

MetaKeyBindingAction
meta_prefs_get_keybinding_action (const char *name)
{
  int i;

  i = G_N_ELEMENTS (key_bindings) - 2; /* -2 for dummy entry at end */
  while (i >= 0)
    {
      if (strcmp (key_bindings[i].name, name) == 0)
        return (MetaKeyBindingAction) i;

      --i;
    }

  return META_KEYBINDING_ACTION_NONE;
}

/* This is used by the menu system to decide what key binding
 * to display next to an option. We return the first non-disabled
 * binding, if any.
 */
void
meta_prefs_get_window_binding (const char          *name,
                               unsigned int        *keysym,
                               MetaVirtualModifier *modifiers)
{
  int i;

  i = G_N_ELEMENTS (key_bindings) - 2; /* -2 for dummy entry at end */
  while (i >= 0)
    {
      if (key_bindings[i].per_window &&
          strcmp (key_bindings[i].name, name) == 0)
        {
          GSList *s = key_bindings[i].bindings;

          while (s)
            {
              MetaKeyCombo *c = s->data;

              if (c->keysym!=0 || c->modifiers!=0)
                {
                  *keysym = c->keysym;
                  *modifiers = c->modifiers;
                  return;
                }

              s = s->next;
            }

          /* Not found; return the disabled value */
          *keysym = *modifiers = 0;
          return;
        }

      --i;
    }

  g_assert_not_reached ();
}

gboolean
meta_prefs_get_compositing_manager (void)
{
  if (use_force_compositor_manager)
    return force_compositor_manager;
  return compositing_manager;
}

gboolean
meta_prefs_get_compositing_fast_alt_tab (void)
{
    return compositing_fast_alt_tab;
}

gboolean
meta_prefs_get_center_new_windows (void)
{
    return center_new_windows;
}

gboolean
meta_prefs_get_allow_tiling ()
{
  return allow_tiling;
}

gboolean
meta_prefs_get_allow_top_tiling ()
{
  return allow_top_tiling;
}

guint
meta_prefs_get_mouse_button_resize (void)
{
  return resize_with_right_button ? 3: 2;
}

guint
meta_prefs_get_mouse_button_menu (void)
{
  return resize_with_right_button ? 2: 3;
}

gboolean
meta_prefs_show_tab_border(void)
{
    return show_tab_border;
}

gboolean
meta_prefs_get_force_fullscreen (void)
{
  return force_fullscreen;
}

MetaPlacementMode
meta_prefs_get_placement_mode (void)
{
  return placement_mode;
}

void
meta_prefs_set_force_compositing_manager (gboolean whether)
{
  use_force_compositor_manager = TRUE;
  force_compositor_manager = whether;
}

void
meta_prefs_set_compositing_fast_alt_tab (gboolean whether)
{
    g_settings_set_boolean (settings_general,
                            KEY_GENERAL_COMPOSITOR_FAST_ALT_TAB,
                            whether);
}

void
meta_prefs_set_center_new_windows (gboolean whether)
{
    g_settings_set_boolean (settings_general,
                            KEY_GENERAL_CENTER_NEW_WINDOWS,
                            whether);
}

void
meta_prefs_set_force_fullscreen (gboolean whether)
{
  force_fullscreen = whether;
}

