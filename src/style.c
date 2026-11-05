// Copyright (c) 2026 Stéphane Gourichon. Written with Claude Code (Anthropic).
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "style.h"

//------------------------------------------------------------------------------

// Problem: GTK 3 has no table of colors per widget state, such as the dark_gc,
// bg_gc and fg_gc of GTK 2, and a theme can restyle any widget state.
// Solution: all the colors of the program are derived here from the named
// colors that GTK 3 themes define, such as @theme_selected_bg_color.
static const char css[] =
  "@define-color superswitcher_workspace shade(@theme_bg_color, 0.7);\n"
  "@define-color superswitcher_workspace_border @theme_base_color;\n"
  "@define-color superswitcher_active_workspace shade(@theme_selected_bg_color, 0.7);\n"
  "@define-color superswitcher_active_workspace_border @theme_selected_bg_color;\n"
  "@define-color superswitcher_window shade(@theme_bg_color, 0.9);\n"
  "@define-color superswitcher_window_border @theme_fg_color;\n"
  "@define-color superswitcher_active_window @theme_selected_bg_color;\n"
  "@define-color superswitcher_active_window_border @theme_selected_fg_color;\n"
  "@define-color superswitcher_drag_outline @theme_text_color;\n"
  "\n"
  // The padding gives the highlight of the active window some room around the
  // icon and the title.
  ".superswitcher-window-row {\n"
  "  padding: 1px 2px;\n"
  "}\n"
  ".superswitcher-active-window {\n"
  "  background-color: @theme_selected_bg_color;\n"
  "  border-radius: 3px;\n"
  "}\n"
  ".superswitcher-active-window label {\n"
  "  color: @theme_selected_fg_color;\n"
  "}\n"
  ".superswitcher-active-window label:disabled {\n"
  "  color: alpha(@theme_selected_fg_color, 0.5);\n"
  "}\n";

//------------------------------------------------------------------------------

void
ss_style_init (void)
{
  GtkCssProvider *provider;
  GError *error;

  provider = gtk_css_provider_new ();
  error = NULL;
  if (!gtk_css_provider_load_from_data (provider, css, -1, &error)) {
    g_warning ("Could not load the style sheet of SuperSwitcher, therefore the "
               "popup will look wrong: %s", error->message);
    g_error_free (error);
  }
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
    GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

//------------------------------------------------------------------------------

void
ss_style_get_color (GtkWidget *widget, const char *name, GdkRGBA *color)
{
  if (!gtk_style_context_lookup_color (gtk_widget_get_style_context (widget), name, color)) {
    g_warning ("The theme does not resolve the color %s, using black in its place.", name);
    gdk_rgba_parse (color, "black");
  }
}
