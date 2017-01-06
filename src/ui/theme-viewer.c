/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Marco theme viewer and test app main() */

/*
 * Copyright (C) 2002 Havoc Pennington
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
#include "util.h"
#include "theme.h"
#include "theme-parser.h"
#include "preview-widget.h"
#include <gtk/gtk.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <libintl.h>
#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x

/* We need to compute all different button arrangements
 * in terms of button location. We don't care about
 * different arrangements in terms of button function.
 *
 * So if dups are allowed, from 0-4 buttons on the left, from 0-4 on
 * the right, 5x5=25 combinations.
 *
 * If no dups, 0-4 on left determines the number on the right plus
 * we have a special case for the "no buttons on either side" case.
 */
#ifndef ALLOW_DUPLICATE_BUTTONS
#define BUTTON_LAYOUT_COMBINATIONS (MAX_BUTTONS_PER_CORNER + 1 + 1)
#else
#define BUTTON_LAYOUT_COMBINATIONS ((MAX_BUTTONS_PER_CORNER+1)*(MAX_BUTTONS_PER_CORNER+1))
#endif

enum
{
  FONT_SIZE_SMALL,
  FONT_SIZE_NORMAL,
  FONT_SIZE_LARGE,
  FONT_SIZE_LAST
};

static MetaTheme *global_theme = NULL;
static GtkWidget *previews[META_FRAME_TYPE_LAST*FONT_SIZE_LAST + BUTTON_LAYOUT_COMBINATIONS] = { NULL, };
static double milliseconds_to_draw_frame = 0.0;

static void run_theme_benchmark (void);


static const gchar *menu_item_string =
  "<ui>\n"
    "<menubar>\n"
      "<menu name='Windows' action='Windows'>\n"
        "<menuitem name='Dialog' action='Dialog'/>\n"
        "<menuitem name='Modal dialog' action='Modal dialog'/>\n"
        "<menuitem name='Utility' action='Utility'/>\n"
        "<menuitem name='Splashscreen' action='Splashscreen'/>\n"
        "<menuitem name='Top dock' action='Top dock'/>\n"
        "<menuitem name='Bottom dock' action='Bottom dock'/>\n"
        "<menuitem name='Left dock' action='Left dock'/>\n"
        "<menuitem name='Right dock' action='Right dock'/>\n"
        "<menuitem name='Desktop' action='Desktop'/>\n"
      "</menu>\n"
    "</menubar>\n"
    "<toolbar>\n"
      "<separator/>\n"
      "<toolitem name='New' action='New'/>\n"
      "<toolitem name='Open' action='Open'/>\n"
      "<toolitem name='Quit' action='Quit'/>\n"
      "<separator/>\n"
    "</toolbar>\n"
  "</ui>\n";

static GtkActionEntry menu_items[] =
{
  { "Windows",           NULL, N_("_Windows"),       NULL,         NULL, NULL },
  { "Dialog",            NULL, N_("_Dialog"),        "<control>d", NULL, NULL },
  { "Modal dialog",      NULL, N_("_Modal dialog"),  NULL,         NULL, NULL },
  { "Utility",           NULL, N_("_Utility"),       "<control>u", NULL, NULL },
  { "Splashscreen",      NULL, N_("_Splashscreen"),  "<control>s", NULL, NULL },
  { "Top dock",          NULL, N_("_Top dock"),      NULL,         NULL, NULL },
  { "Bottom dock",       NULL, N_("_Bottom dock"),   NULL,         NULL, NULL },
  { "Left dock",         NULL, N_("_Left dock"),     NULL,         NULL, NULL },
  { "Right dock",        NULL, N_("_Right dock"),    NULL,         NULL, NULL },
  { "All docks",         NULL, N_("_All docks"),     NULL,         NULL, NULL },
  { "Desktop",           NULL, N_("Des_ktop"),       NULL,         NULL, NULL }
};

static GtkActionEntry tool_items[] =
{
  { "New",      GTK_STOCK_NEW,  NULL,   NULL,
    N_("Open another one of these windows"),            NULL },
  { "Open",     GTK_STOCK_OPEN, NULL,   NULL,
    N_("This is a demo button with an 'open' icon"),    NULL },
  { "Quit",     GTK_STOCK_QUIT, NULL,   NULL,
    N_("This is a demo button with a 'quit' icon"),     NULL }
};

static GtkWidget *
normal_contents (void)
{
  GtkWidget *table;
  GtkWidget *handlebox;
  GtkWidget *statusbar;
  GtkWidget *contents;
  GtkWidget *sw;
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;

  table = gtk_table_new (1, 4, FALSE);

  /* Create the menubar
   */

  action_group = gtk_action_group_new ("mainmenu");
  gtk_action_group_add_actions (action_group,
                                menu_items,
                                G_N_ELEMENTS (menu_items),
                                NULL);
  gtk_action_group_add_actions (action_group,
                                tool_items,
                                G_N_ELEMENTS (tool_items),
                                NULL);

  ui_manager = gtk_ui_manager_new ();

  gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

  /* create menu items */
  gtk_ui_manager_add_ui_from_string (ui_manager, menu_item_string, -1, NULL);

  gtk_table_attach (GTK_TABLE (table),
                    gtk_ui_manager_get_widget (ui_manager, "/ui/menubar"),
                    /* X direction */          /* Y direction */
                    0, 1,                      0, 1,
                    GTK_EXPAND | GTK_FILL,     0,
                    0,                         0);

  handlebox = gtk_handle_box_new ();

  gtk_container_add (GTK_CONTAINER (handlebox),
                     gtk_ui_manager_get_widget (ui_manager, "/ui/toolbar"));

  gtk_table_attach (GTK_TABLE (table),
                    handlebox,
                    /* X direction */       /* Y direction */
                    0, 1,                   1, 2,
                    GTK_EXPAND | GTK_FILL,  0,
                    0,                      0);

  /* Create document
   */

  sw = gtk_scrolled_window_new (NULL, NULL);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_IN);

  gtk_table_attach (GTK_TABLE (table),
                    sw,
                    /* X direction */       /* Y direction */
                    0, 1,                   2, 3,
                    GTK_EXPAND | GTK_FILL,  GTK_EXPAND | GTK_FILL,
                    0,                      0);

  contents = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (contents),
                               PANGO_WRAP_WORD);

  gtk_container_add (GTK_CONTAINER (sw),
                     contents);

  /* Create statusbar */

  statusbar = gtk_statusbar_new ();
  gtk_table_attach (GTK_TABLE (table),
                    statusbar,
                    /* X direction */       /* Y direction */
                    0, 1,                   3, 4,
                    GTK_EXPAND | GTK_FILL,  0,
                    0,                      0);

  gtk_widget_show_all (table);

  g_object_unref (ui_manager);

  return table;
}

static void
update_spacings (GtkWidget *vbox,
                 GtkWidget *action_area)
{
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_box_set_spacing (GTK_BOX (action_area), 10);
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
}

static GtkWidget*
dialog_contents (void)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *action_area;
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *button;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  action_area = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);

  gtk_button_box_set_layout (GTK_BUTTON_BOX (action_area),
                             GTK_BUTTONBOX_END);

  button = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_box_pack_end (GTK_BOX (action_area),
                    button,
                    FALSE, TRUE, 0);

  gtk_box_pack_end (GTK_BOX (vbox), action_area,
                    FALSE, TRUE, 0);

  update_spacings (vbox, action_area);

  label = gtk_label_new (_("This is a sample message in a sample dialog"));
  image = gtk_image_new_from_icon_name ("dialog-information",
                                        GTK_ICON_SIZE_DIALOG);
  gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (image, GTK_ALIGN_START);

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  gtk_box_pack_start (GTK_BOX (hbox), image,
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), label,
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (vbox),
                      hbox,
                      FALSE, FALSE, 0);

  gtk_widget_show_all (vbox);

  return vbox;
}

static GtkWidget*
utility_contents (void)
{
  GtkWidget *table;
  GtkWidget *button;
  int i, j;

  table = gtk_table_new (3, 4, FALSE);

  i = 0;
  while (i < 3)
    {
      j = 0;
      while (j < 4)
        {
          char *str;

          str = g_strdup_printf ("_%c", (char) ('A' + 4*i + j));

          button = gtk_button_new_with_mnemonic (str);

          g_free (str);

          gtk_table_attach (GTK_TABLE (table),
                            button,
                            /* X direction */       /* Y direction */
                            i, i+1,                   j, j+1,
                            GTK_EXPAND | GTK_FILL,  GTK_EXPAND | GTK_FILL,
                            0,                      0);

          ++j;
        }

      ++i;
    }

  gtk_widget_show_all (table);

  return table;
}

static GtkWidget*
menu_contents (void)
{
  GtkWidget *vbox;
  GtkWidget *mi;
  int i;
  GtkWidget *frame;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame),
                             GTK_SHADOW_OUT);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  i = 0;
  while (i < 10)
    {
      char *str = g_strdup_printf (_("Fake menu item %d\n"), i + 1);
      mi = gtk_label_new (str);
      gtk_widget_set_halign (mi, GTK_ALIGN_START);
      gtk_widget_set_valign (mi, GTK_ALIGN_CENTER);
      g_free (str);
      gtk_box_pack_start (GTK_BOX (vbox), mi, FALSE, FALSE, 0);

      ++i;
    }

  gtk_container_add (GTK_CONTAINER (frame), vbox);

  gtk_widget_show_all (frame);

  return frame;
}

static GtkWidget*
border_only_contents (void)
{
  GtkWidget *event_box;
  GtkWidget *vbox;
  GtkWidget *w;
  GdkColor color;

  event_box = gtk_event_box_new ();

  color.red = 40000;
  color.green = 0;
  color.blue = 40000;
  gtk_widget_modify_bg (event_box, GTK_STATE_NORMAL, &color);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  w = gtk_label_new (_("Border-only window"));
  gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);
  w = gtk_button_new_with_label (_("Bar"));
  gtk_box_pack_start (GTK_BOX (vbox), w, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (event_box), vbox);

  gtk_widget_show_all (event_box);

  return event_box;
}

static GtkWidget*
get_window_contents (MetaFrameType  type,
                     const char   **title)
{
  switch (type)
    {
    case META_FRAME_TYPE_NORMAL:
      *title = _("Normal Application Window");
      return normal_contents ();

    case META_FRAME_TYPE_DIALOG:
      *title = _("Dialog Box");
      return dialog_contents ();

    case META_FRAME_TYPE_MODAL_DIALOG:
      *title = _("Modal Dialog Box");
      return dialog_contents ();

    case META_FRAME_TYPE_UTILITY:
      *title = _("Utility Palette");
      return utility_contents ();

    case META_FRAME_TYPE_MENU:
      *title = _("Torn-off Menu");
      return menu_contents ();

    case META_FRAME_TYPE_BORDER:
      *title = _("Border");
      return border_only_contents ();

    case META_FRAME_TYPE_LAST:
      g_assert_not_reached ();
      break;
    }

  return NULL;
}

static MetaFrameFlags
get_window_flags (MetaFrameType type)
{
  MetaFrameFlags flags;

  flags = META_FRAME_ALLOWS_DELETE |
    META_FRAME_ALLOWS_MENU |
    META_FRAME_ALLOWS_MINIMIZE |
    META_FRAME_ALLOWS_MAXIMIZE |
    META_FRAME_ALLOWS_VERTICAL_RESIZE |
    META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
    META_FRAME_HAS_FOCUS |
    META_FRAME_ALLOWS_SHADE |
    META_FRAME_ALLOWS_MOVE;

  switch (type)
    {
    case META_FRAME_TYPE_NORMAL:
      break;

    case META_FRAME_TYPE_DIALOG:
    case META_FRAME_TYPE_MODAL_DIALOG:
      flags &= ~(META_FRAME_ALLOWS_MINIMIZE |
                 META_FRAME_ALLOWS_MAXIMIZE);
      break;

    case META_FRAME_TYPE_UTILITY:
      flags &= ~(META_FRAME_ALLOWS_MINIMIZE |
                 META_FRAME_ALLOWS_MAXIMIZE);
      break;

    case META_FRAME_TYPE_MENU:
      flags &= ~(META_FRAME_ALLOWS_MINIMIZE |
                 META_FRAME_ALLOWS_MAXIMIZE);
      break;

    case META_FRAME_TYPE_BORDER:
      break;

    case META_FRAME_TYPE_LAST:
      g_assert_not_reached ();
      break;
    }

  return flags;
}

static GtkWidget*
preview_collection (int font_size,
                    PangoFontDescription *base_desc)
{
  GtkWidget *box;
  GtkWidget *sw;
  GdkColor desktop_color;
  int i;
  GtkWidget *eventbox;

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_spacing (GTK_BOX (box), 20);
  gtk_container_set_border_width (GTK_CONTAINER (box), 20);

  eventbox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (eventbox), box);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), eventbox);

  desktop_color.red = 0x5144;
  desktop_color.green = 0x75D6;
  desktop_color.blue = 0xA699;

  gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &desktop_color);

  i = 0;
  while (i < META_FRAME_TYPE_LAST)
    {
      const char *title = NULL;
      GtkWidget *contents;
      GtkWidget *align;
      double xalign, yalign;
      GtkWidget *eventbox2;
      GtkWidget *preview;
      PangoFontDescription *font_desc;
      double scale;

      eventbox2 = gtk_event_box_new ();

      preview = meta_preview_new ();

      gtk_container_add (GTK_CONTAINER (eventbox2), preview);

      meta_preview_set_frame_type (META_PREVIEW (preview), i);
      meta_preview_set_frame_flags (META_PREVIEW (preview),
                                    get_window_flags (i));

      meta_preview_set_theme (META_PREVIEW (preview), global_theme);

      contents = get_window_contents (i, &title);

      meta_preview_set_title (META_PREVIEW (preview), title);

      gtk_container_add (GTK_CONTAINER (preview), contents);

      if (i == META_FRAME_TYPE_MENU)
        {
          xalign = 0.0;
          yalign = 0.0;
        }
      else
        {
          xalign = 0.5;
          yalign = 0.5;
        }

      align = gtk_alignment_new (0.0, 0.0, xalign, yalign);
      gtk_container_add (GTK_CONTAINER (align), eventbox2);

      gtk_box_pack_start (GTK_BOX (box), align, TRUE, TRUE, 0);

      switch (font_size)
        {
        case FONT_SIZE_SMALL:
          scale = PANGO_SCALE_XX_SMALL;
          break;
        case FONT_SIZE_LARGE:
          scale = PANGO_SCALE_XX_LARGE;
          break;
        default:
          scale = 1.0;
          break;
        }

      if (scale != 1.0)
        {
          font_desc = pango_font_description_new ();

          pango_font_description_set_size (font_desc,
                                           MAX (pango_font_description_get_size (base_desc) * scale, 1));

          gtk_widget_modify_font (preview, font_desc);

          pango_font_description_free (font_desc);
        }

      previews[font_size*META_FRAME_TYPE_LAST + i] = preview;

      ++i;
    }

  return sw;
}

static MetaButtonLayout different_layouts[BUTTON_LAYOUT_COMBINATIONS];

static void
init_layouts (void)
{
  int i;

  /* Blank out all the layouts */
  i = 0;
  while (i < (int) G_N_ELEMENTS (different_layouts))
    {
      int j;

      j = 0;
      while (j < MAX_BUTTONS_PER_CORNER)
        {
          different_layouts[i].left_buttons[j] = META_BUTTON_FUNCTION_LAST;
          different_layouts[i].right_buttons[j] = META_BUTTON_FUNCTION_LAST;
          ++j;
        }
      ++i;
    }

#ifndef ALLOW_DUPLICATE_BUTTONS
  i = 0;
  while (i <= MAX_BUTTONS_PER_CORNER)
    {
      int j;

      j = 0;
      while (j < i)
        {
          different_layouts[i].right_buttons[j] = (MetaButtonFunction) j;
          ++j;
        }
      while (j < MAX_BUTTONS_PER_CORNER)
        {
          different_layouts[i].left_buttons[j-i] = (MetaButtonFunction) j;
          ++j;
        }

      ++i;
    }

  /* Special extra case for no buttons on either side */
  different_layouts[i].left_buttons[0] = META_BUTTON_FUNCTION_LAST;
  different_layouts[i].right_buttons[0] = META_BUTTON_FUNCTION_LAST;

#else
  /* FIXME this code is if we allow duplicate buttons,
   * which we currently do not
   */
  int left;
  int i;

  left = 0;
  i = 0;

  while (left < MAX_BUTTONS_PER_CORNER)
    {
      int right;

      right = 0;

      while (right < MAX_BUTTONS_PER_CORNER)
        {
          int j;

          static MetaButtonFunction left_functions[MAX_BUTTONS_PER_CORNER] = {
            META_BUTTON_FUNCTION_MENU,
            META_BUTTON_FUNCTION_MINIMIZE,
            META_BUTTON_FUNCTION_MAXIMIZE,
            META_BUTTON_FUNCTION_CLOSE
          };
          static MetaButtonFunction right_functions[MAX_BUTTONS_PER_CORNER] = {
            META_BUTTON_FUNCTION_MINIMIZE,
            META_BUTTON_FUNCTION_MAXIMIZE,
            META_BUTTON_FUNCTION_CLOSE,
            META_BUTTON_FUNCTION_MENU
          };

          g_assert (i < BUTTON_LAYOUT_COMBINATIONS);

          j = 0;
          while (j <= left)
            {
              different_layouts[i].left_buttons[j] = left_functions[j];
              ++j;
            }

          j = 0;
          while (j <= right)
            {
              different_layouts[i].right_buttons[j] = right_functions[j];
              ++j;
            }

          ++i;

          ++right;
        }

      ++left;
    }
#endif
}


static GtkWidget*
previews_of_button_layouts (void)
{
  static gboolean initted = FALSE;
  GtkWidget *box;
  GtkWidget *sw;
  GdkColor desktop_color;
  int i;
  GtkWidget *eventbox;

  if (!initted)
    {
      init_layouts ();
      initted = TRUE;
    }

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_spacing (GTK_BOX (box), 20);
  gtk_container_set_border_width (GTK_CONTAINER (box), 20);

  eventbox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (eventbox), box);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), eventbox);

  desktop_color.red = 0x5144;
  desktop_color.green = 0x75D6;
  desktop_color.blue = 0xA699;

  gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &desktop_color);

  i = 0;
  while (i < BUTTON_LAYOUT_COMBINATIONS)
    {
      GtkWidget *align;
      double xalign, yalign;
      GtkWidget *eventbox2;
      GtkWidget *preview;
      char *title;

      eventbox2 = gtk_event_box_new ();

      preview = meta_preview_new ();

      gtk_container_add (GTK_CONTAINER (eventbox2), preview);

      meta_preview_set_theme (META_PREVIEW (preview), global_theme);

      title = g_strdup_printf (_("Button layout test %d"), i+1);
      meta_preview_set_title (META_PREVIEW (preview), title);
      g_free (title);

      meta_preview_set_button_layout (META_PREVIEW (preview),
                                      &different_layouts[i]);

      xalign = 0.5;
      yalign = 0.5;

      align = gtk_alignment_new (0.0, 0.0, xalign, yalign);
      gtk_container_add (GTK_CONTAINER (align), eventbox2);

      gtk_box_pack_start (GTK_BOX (box), align, TRUE, TRUE, 0);

      previews[META_FRAME_TYPE_LAST*FONT_SIZE_LAST + i] = preview;

      ++i;
    }

  return sw;
}

static GtkWidget*
benchmark_summary (void)
{
  char *msg;
  GtkWidget *label;

  msg = g_strdup_printf (_("%g milliseconds to draw one window frame"),
                         milliseconds_to_draw_frame);
  label = gtk_label_new (msg);
  g_free (msg);

  return label;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *collection;
  GtkStyle *style;
  GError *err;
  clock_t start, end;
  GtkWidget *notebook;
  int i;

  bindtextdomain (GETTEXT_PACKAGE, MARCO_LOCALEDIR);
  textdomain(GETTEXT_PACKAGE);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

  gtk_init (&argc, &argv);

  if (g_getenv ("MARCO_DEBUG") != NULL)
    {
      meta_set_debugging (TRUE);
      meta_set_verbose (TRUE);
    }

  start = clock ();
  err = NULL;
  if (argc == 1)
    global_theme = meta_theme_load ("ClearlooksRe", &err);
  else if (argc == 2)
    global_theme = meta_theme_load (argv[1], &err);
  else
    {
      g_printerr (_("Usage: marco-theme-viewer [THEMENAME]\n"));
      exit (1);
    }
  end = clock ();

  if (global_theme == NULL)
    {
      g_printerr (_("Error loading theme: %s\n"),
                  err->message);
      g_error_free (err);
      exit (1);
    }

  g_print (_("Loaded theme \"%s\" in %g seconds\n"),
           global_theme->name,
           (end - start) / (double) CLOCKS_PER_SEC);

  run_theme_benchmark ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 350, 350);

  if (strcmp (global_theme->name, global_theme->readable_name)==0)
    gtk_window_set_title (GTK_WINDOW (window),
                          global_theme->readable_name);
  else
    {
      /* The theme directory name is different from the name the theme
       * gives itself within its file.  Display both, directory name first.
       */
      gchar *title =  g_strconcat (global_theme->name, " - ",
                                   global_theme->readable_name,
                                   NULL);

      gtk_window_set_title (GTK_WINDOW (window),
                            title);

      g_free (title);
    }

  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_realize (window);
  style = gtk_widget_get_style (window);

  g_assert (style);
  g_assert (style->font_desc);

  notebook = gtk_notebook_new ();
  gtk_container_add (GTK_CONTAINER (window), notebook);

  collection = preview_collection (FONT_SIZE_NORMAL,
                                   style->font_desc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            collection,
                            gtk_label_new (_("Normal Title Font")));

  collection = preview_collection (FONT_SIZE_SMALL,
                                   style->font_desc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            collection,
                            gtk_label_new (_("Small Title Font")));

  collection = preview_collection (FONT_SIZE_LARGE,
                                   style->font_desc);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            collection,
                            gtk_label_new (_("Large Title Font")));

  collection = previews_of_button_layouts ();
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            collection,
                            gtk_label_new (_("Button Layouts")));

  collection = benchmark_summary ();
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            collection,
                            gtk_label_new (_("Benchmark")));

  i = 0;
  while (i < (int) G_N_ELEMENTS (previews))
    {
      /* preview widget likes to be realized before its size request.
       * it's lame that way.
       */
      gtk_widget_realize (previews[i]);

      ++i;
    }

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}


static MetaFrameFlags
get_flags (GtkWidget *widget)
{
  return META_FRAME_ALLOWS_DELETE |
    META_FRAME_ALLOWS_MENU |
    META_FRAME_ALLOWS_MINIMIZE |
    META_FRAME_ALLOWS_MAXIMIZE |
    META_FRAME_ALLOWS_VERTICAL_RESIZE |
    META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
    META_FRAME_HAS_FOCUS |
    META_FRAME_ALLOWS_SHADE |
    META_FRAME_ALLOWS_MOVE;
}

static int
get_text_height (GtkWidget *widget)
{
  return meta_pango_font_desc_get_text_height (gtk_widget_get_style (widget)->font_desc,
                                               gtk_widget_get_pango_context (widget));
}

static PangoLayout*
create_title_layout (GtkWidget *widget)
{
  PangoLayout *layout;

  layout = gtk_widget_create_pango_layout (widget, _("Window Title Goes Here"));

  return layout;
}

static void
run_theme_benchmark (void)
{
  GtkWidget* widget;
  cairo_surface_t *pixmap;
  cairo_t *cr;
  int top_height, bottom_height, left_width, right_width;
  MetaButtonState button_states[META_BUTTON_TYPE_LAST] =
  {
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL
  };
  PangoLayout *layout;
  clock_t start;
  clock_t end;
  GTimer *timer;
  int i;
  MetaButtonLayout button_layout;
#define ITERATIONS 100
  int client_width;
  int client_height;
  int inc;

  widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_realize (widget);

  meta_theme_get_frame_borders (global_theme,
                                META_FRAME_TYPE_NORMAL,
                                get_text_height (widget),
                                get_flags (widget),
                                &top_height,
                                &bottom_height,
                                &left_width,
                                &right_width);

  layout = create_title_layout (widget);

  i = 0;
  while (i < MAX_BUTTONS_PER_CORNER)
    {
      button_layout.left_buttons[i] = META_BUTTON_FUNCTION_LAST;
      button_layout.right_buttons[i] = META_BUTTON_FUNCTION_LAST;
      ++i;
    }

  button_layout.left_buttons[0] = META_BUTTON_FUNCTION_MENU;

  button_layout.right_buttons[0] = META_BUTTON_FUNCTION_MINIMIZE;
  button_layout.right_buttons[1] = META_BUTTON_FUNCTION_MAXIMIZE;
  button_layout.right_buttons[2] = META_BUTTON_FUNCTION_CLOSE;

  timer = g_timer_new ();
  start = clock ();

  client_width = 50;
  client_height = 50;
  inc = 1000 / ITERATIONS; /* Increment to grow width/height,
                            * eliminates caching effects.
                            */

  i = 0;
  while (i < ITERATIONS)
    {
      /* Creating the pixmap in the loop is right, since
       * GDK does the same with its double buffering.
       */
      pixmap = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                                  CAIRO_CONTENT_COLOR,
                                                  client_width + left_width + right_width,
                                                  client_height + top_height + bottom_height);
      cr = cairo_create (pixmap);

      meta_theme_draw_frame (global_theme,
                             widget,
                             cr,
                             META_FRAME_TYPE_NORMAL,
                             get_flags (widget),
                             client_width, client_height,
                             layout,
                             get_text_height (widget),
                             &button_layout,
                             button_states,
                             meta_preview_get_mini_icon (),
                             meta_preview_get_icon ());

      cairo_destroy (cr);
      cairo_surface_destroy (pixmap);

      ++i;
      client_width += inc;
      client_height += inc;
    }

  end = clock ();
  g_timer_stop (timer);

  milliseconds_to_draw_frame = (g_timer_elapsed (timer, NULL) / (double) ITERATIONS) * 1000;

  g_print (_("Drew %d frames in %g client-side seconds (%g milliseconds per frame) and %g seconds wall clock time including X server resources (%g milliseconds per frame)\n"),
           ITERATIONS,
           ((double)end - (double)start) / CLOCKS_PER_SEC,
           (((double)end - (double)start) / CLOCKS_PER_SEC / (double) ITERATIONS) * 1000,
           g_timer_elapsed (timer, NULL),
           milliseconds_to_draw_frame);

  g_timer_destroy (timer);
  g_object_unref (G_OBJECT (layout));
  gtk_widget_destroy (widget);

#undef ITERATIONS
}

