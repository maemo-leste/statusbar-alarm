#include <clock-plugin-dbus.h>
#include <time.h>
#include <clockd/libtime.h>
#include <gconf/gconf-client.h>
#include <hildon/hildon-helper.h>
#include <hildon/hildon-gtk.h>
#include <libhildondesktop/libhildondesktop.h>
#include <libosso.h>

#include <libintl.h>
#include <string.h>
#include <math.h>

#include "config.h"

#define _(x) dgettext("hildon-libs", x)

#define CLOCK_ICON_SIZE_AREA 18
#define _TIME_CLOCK_FONT 21

#define CLOCK_PLUGIN_TYPE (clock_plugin_get_type())
#define CLOCK_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLOCK_PLUGIN_TYPE, ClockPlugin))
#define CLOCK_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), CLOCK_PLUGIN_TYPE, ClockPluginPrivate))

typedef struct _ClockPlugin ClockPlugin;
typedef struct _ClockPluginClass ClockPluginClass;
typedef struct _ClockPluginPrivate ClockPluginPrivate;

struct _ClockPlugin
{
  HDStatusMenuItem parent;
  ClockPluginPrivate *priv;
};

struct _ClockPluginClass
{
  HDStatusMenuItemClass parent;
};

struct _ClockPluginPrivate
{
  GtkWidget *button;
  GtkWidget *clock_image;
  GtkWidget *label3;
  GtkWidget *hbox;
  GtkWidget *time_label;
  GtkWidget *align2;
  GtkWidget *status_bar_alarm_image;
  GtkWidget *apm_label;
  GtkWidget *align1;
  GtkWidget *vbox;
  gboolean alarm_icon_visible;
  GdkPixbuf *alarm_on_icon;
  GdkPixbuf *icon_night_time;
  GdkPixbuf *icon_day_time;
  GdkPixbuf *icon_alarm_on;
  int hour;
  int min;
  GdkColor clock_color;
  gboolean time_fmt_24h;
  guint time_changed_id;
  guint gc_notify;
  int field_5C;
  DBusConnection *session_bus;
  DBusConnection *system_bus;
  osso_context_t *osso;
  osso_display_state_t display_state;
};

HD_DEFINE_PLUGIN_MODULE(ClockPlugin, clock_plugin, HD_TYPE_STATUS_MENU_ITEM)

static void
clock_plugin_class_finalize(ClockPluginClass *klass)
{
}

static void
_draw_status_menu_clock(ClockPlugin *plugin)
{
  ClockPluginPrivate *priv = CLOCK_PLUGIN(plugin)->priv;
  char tmbuf[512];
  struct tm tm;

  const char *time_text;
  gint label_state;

  if (time_get_local(&tm))
    return;

  const gchar *s;

  time_format_time(&tm, _("wdgt_va_date_long"), tmbuf, sizeof(tmbuf) - 1);

  s = gtk_label_get_text(GTK_LABEL(priv->label3));

  if (s && !strcmp(s, tmbuf))
    time_text = gtk_label_get_text(GTK_LABEL(priv->time_label));
  else
  {
    gtk_label_set_text(GTK_LABEL(priv->label3), tmbuf);
    time_text = gtk_label_get_text(GTK_LABEL(priv->time_label));
  }

  if (priv->time_fmt_24h != 1)
  {
    gchar *fmt = g_strconcat(_("wdgt_va_12h_hours"), ":", _("wdgt_va_minutes"),
                             NULL);

    time_format_time(&tm, fmt, tmbuf, sizeof(tmbuf) - 1);
    g_free(fmt);

    if (!time_text || strcmp(time_text, tmbuf))
      gtk_label_set_text(GTK_LABEL(priv->time_label), tmbuf);

    if (tm.tm_hour < 12)
      label_state = 1;
    else
      label_state = 2;

    if (label_state !=
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(priv->apm_label),
                                          "label-state")))
    {
      gtk_label_set_text(
            GTK_LABEL(priv->apm_label),
            label_state == 1 ? _("wdgt_va_am") : _("wdgt_va_pm"));
      g_object_set_data(G_OBJECT(priv->apm_label),
                        "label-state", GINT_TO_POINTER(label_state));
    }
  }
  else
  {
    time_format_time(&tm, _("wdgt_va_24h_time"), tmbuf, sizeof(tmbuf) - 1);

    if (!time_text || strcmp(time_text, tmbuf))
      gtk_label_set_text(GTK_LABEL(priv->time_label), tmbuf);
  }

  priv->hour = tm.tm_hour;
  priv->min = tm.tm_min;

  if (priv->alarm_icon_visible)
  {
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->clock_image),
                              priv->icon_alarm_on);
  }
  else if (tm.tm_hour >=6 && tm.tm_hour <= 17)
  {
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->clock_image),
                              priv->icon_night_time);
    gdk_color_parse("White", &priv->clock_color);
  }
  else
  {
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->clock_image),
                              priv->icon_day_time);
    gdk_color_parse("Black", &priv->clock_color);
  }
}

static void
_show_status_menu_clock(ClockPlugin *plugin)
{
  ClockPluginPrivate *priv = CLOCK_PLUGIN(plugin)->priv;

  if (!priv->alarm_icon_visible && priv->time_fmt_24h)
    gtk_widget_hide(GTK_WIDGET(priv->vbox));
  else
    gtk_widget_show(GTK_WIDGET(priv->vbox));

  _draw_status_menu_clock(plugin);
}

static void
_show_status_menu_widget(ClockPlugin *plugin, gboolean show)
{
  ClockPluginPrivate *priv = CLOCK_PLUGIN(plugin)->priv;

  priv->alarm_icon_visible = show;

  if (show && priv->alarm_on_icon)
  {
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->status_bar_alarm_image),
                              priv->alarm_on_icon);
    gtk_misc_set_alignment(GTK_MISC(priv->status_bar_alarm_image), 0.5, 0.5);
    gtk_alignment_set_padding(GTK_ALIGNMENT(priv->align1), 0, 0, 0, 0);
  }
  else
  {
    gtk_image_clear(GTK_IMAGE(priv->status_bar_alarm_image));
    gtk_misc_set_alignment(GTK_MISC(priv->status_bar_alarm_image), 0.5, 0.5);
    gtk_alignment_set_padding(GTK_ALIGNMENT(priv->align1), 14, 0, 0, 0);
  }

  _show_status_menu_clock(plugin);
}

static gint
_alarms_changed_cb(const gchar *interface, const gchar *method, GArray *arguments,
                gpointer data, osso_rpc_t *retval)
{
  if (!g_ascii_strcasecmp(method, STATUSAREA_CLOCK_ALARM_SET))
    _show_status_menu_widget(data, TRUE);
  else if (!g_ascii_strcasecmp(method, STATUSAREA_CLOCK_NO_ALARMS))
    _show_status_menu_widget(data, FALSE);

  return 0;
}

static gboolean
_idle_update_time_cb(gpointer user_data)
{
  static struct tm last_update_time;
  ClockPlugin *plugin = user_data;
  ClockPluginPrivate *priv = CLOCK_PLUGIN(plugin)->priv;

  time_get_local(&last_update_time);

  priv->time_changed_id =
      gdk_threads_add_timeout(1000 * (60 - last_update_time.tm_sec) + 500,
                            _idle_update_time_cb, plugin);

  _draw_status_menu_clock(plugin);

  return FALSE;
}

static DBusHandlerResult
_dbus_filter(DBusConnection *connection, DBusMessage *message, void *user_data)
{
  ClockPlugin *plugin = user_data;

  if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_SIGNAL)
  {
    if (!g_strcmp0(dbus_message_get_member(message), CLOCKD_TIME_CHANGED))
    {
      ClockPluginPrivate *priv = CLOCK_PLUGIN(plugin)->priv;

      time_get_synced();

      if (priv->time_changed_id)
        g_source_remove(priv->time_changed_id);

      if (priv->display_state == OSSO_DISPLAY_OFF)
        priv->time_changed_id = 0;
      else
      {
        priv->time_changed_id =
            gdk_threads_add_idle(_idle_update_time_cb, plugin);
      }
    }
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
_clock_plugin_dispose(GObject *object)
{
  ClockPluginPrivate *priv = CLOCK_PLUGIN(object)->priv;

  if (priv->time_changed_id)
  {
    g_source_remove(priv->time_changed_id);
    priv->time_changed_id = 0;
  }

  if (priv->gc_notify)
  {
    gconf_client_notify_remove(gconf_client_get_default(), priv->gc_notify);
    priv->gc_notify = 0;
  }

  if (priv->osso)
  {
    osso_rpc_unset_cb_f(priv->osso, STATUSAREA_CLOCK_SERVICE,
                        STATUSAREA_CLOCK_PATH, STATUSAREA_CLOCK_INTERFACE,
                        _alarms_changed_cb, object);
  }

  if (priv->session_bus)
  {
    g_debug("Close connection '%s' to Session Bus",
            dbus_bus_get_unique_name(priv->session_bus));
    dbus_connection_unref(priv->session_bus);
    priv->session_bus = NULL;
  }

  if (priv->system_bus)
  {
    g_debug("Close connection '%s' to Session Bus",
            dbus_bus_get_unique_name(priv->system_bus));
    dbus_connection_remove_filter(priv->system_bus, _dbus_filter, object);
    dbus_connection_unref(priv->system_bus);
    priv->system_bus = NULL;
  }

  if (priv->osso)
  {
    osso_deinitialize(priv->osso);
    priv->osso = NULL;
  }

  G_OBJECT_CLASS(clock_plugin_parent_class)->dispose(object);
}

static void
_gc_time_format_changed_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry,
                        gpointer user_data)
{
  ClockPlugin *plugin = user_data;
  ClockPluginPrivate *priv = plugin->priv;

  priv->time_fmt_24h =
      gconf_client_get_bool(client, "/apps/clock/time-format", NULL);

  if (priv->time_fmt_24h)
    gtk_widget_hide(GTK_WIDGET(priv->align1));
  else
    gtk_widget_show(GTK_WIDGET(priv->align1));

  _show_status_menu_clock(plugin);
  _draw_status_menu_clock(plugin);
}

static void
_handle_signals_cb(osso_display_state_t state, gpointer data)
{
  ClockPlugin *plugin = data;
  ClockPluginPrivate *priv = plugin->priv;

  priv->display_state = state;

  if (state == OSSO_DISPLAY_OFF)
  {
    if (priv->time_changed_id)
    {
      g_source_remove(priv->time_changed_id);
      priv->time_changed_id = 0;
    }
  }
  else if (!priv->time_changed_id)
    priv->time_changed_id = gdk_threads_add_idle(_idle_update_time_cb, plugin);
}

static gboolean
_clock_image_expose_event_cb(GtkWidget *widget, GdkEvent *event,
                             gpointer user_data)
{
  ClockPlugin *plugin = user_data;
  ClockPluginPrivate *priv = plugin->priv;
  cairo_t *cr;
  GdkColor color;

  if (!priv->alarm_icon_visible)
  {
    double tmp;
    double hour_angle;
    double min_angle; // d12
    int sx = widget->allocation.x + 24 +
        (widget->allocation.width - widget->requisition.width) / 2;
    int sy = widget->allocation.y + 24 +
        (widget->allocation.height - widget->requisition.height) / 2;
    double hour_fraction = (double)priv->min / 60.0;

    tmp = hour_fraction - 0.25;

    if (tmp > 1.0)
      tmp = tmp - 1.0;

    if (tmp < 0.0)
      tmp = tmp + 1.0;

    hour_angle = (tmp + tmp) * 3.14159265;

    tmp = (hour_fraction + (double)(priv->hour % 12 - 3)) / 12.0;

    min_angle = (tmp + tmp) * 3.14159265;

    color = priv->clock_color;

    cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_new_path(cr);

    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, sx, sy);

    cairo_line_to(cr, sx + (int)cos(hour_angle) * 16.0,
                  sy + (int)(sin(hour_angle) * 16.0));
    gdk_cairo_set_source_color(cr, &color);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, sx, sy);

    cairo_line_to(cr, sx + (int)(cos(min_angle) * 13.0),
                  sy + (int)(sin(min_angle) * 13.0));
    gdk_cairo_set_source_color(cr, &color);
    cairo_stroke(cr);
    cairo_restore(cr);
    cairo_destroy(cr);
  }

  return FALSE;
}

static void
_cpa_screen_size_changed_cb(GdkScreen *screen, gpointer user_data)
{
  ClockPluginPrivate *priv = user_data;
  int size = 21 * PANGO_SCALE;
  PangoAttrList *attrs = gtk_label_get_attributes(GTK_LABEL(priv->time_label));

  if (gdk_screen_get_width(screen) < gdk_screen_get_height(screen))
  {
    GtkStyle *style =
        gtk_rc_get_style_by_paths(gtk_settings_get_default(),
                                  "SystemFont", NULL, G_TYPE_NONE);

    size = pango_font_description_get_size(style->font_desc);
  }

  pango_attr_list_change(attrs, pango_attr_size_new(size));
  gtk_label_set_attributes(GTK_LABEL(priv->time_label), attrs);
}

static void
_status_menu_button_clicked_cb(GtkButton *button, gpointer user_data)
{
  ClockPlugin *plugin = user_data;
  ClockPluginPrivate *priv = plugin->priv;

  osso_rpc_run(priv->osso,
               "com.nokia.worldclock",
               "/com/nokia/worldclock",
               "com.nokia.worldclock",
               "top_application",
               NULL,
               DBUS_TYPE_INVALID);
}

static void
_time_label_style_set_cb(GtkWidget *widget, GtkStyle *previous_style,
                         gpointer user_data)
{
  ClockPlugin *plugin = user_data;
  ClockPluginPrivate *priv = plugin->priv;
  GtkStyle *style = gtk_rc_get_style_by_paths(gtk_settings_get_default(),
                                              "SystemFont", 0, G_TYPE_NONE);
  if (style && style->font_desc)
  {
    PangoFontDescription *font_desc =
        pango_font_description_copy(style->font_desc);
    PangoAttrList *attrs = pango_attr_list_new();

    pango_font_description_set_size(font_desc, 21 * PANGO_SCALE);
    pango_attr_list_insert(attrs, pango_attr_font_desc_new(font_desc));
    gtk_label_set_attributes(GTK_LABEL(priv->time_label), attrs);
    pango_attr_list_unref(attrs);
    pango_font_description_free(font_desc);
  }
}

static void
clock_plugin_class_init(ClockPluginClass *klass)
{
  G_OBJECT_CLASS(klass)->dispose = _clock_plugin_dispose;

  g_type_class_add_private(klass, sizeof(ClockPluginPrivate));
}

static void
_create_status_area_widgets(ClockPluginPrivate *priv)
{
  PangoAttrList *attrs;
  GtkIconTheme *icon_theme = gtk_icon_theme_get_default();

  priv->hbox = gtk_hbox_new(FALSE, 4);
  priv->time_label = gtk_label_new("");

  gtk_misc_set_padding(GTK_MISC(priv->time_label), 0, 0);
  gtk_label_set_justify(GTK_LABEL(priv->time_label), GTK_JUSTIFY_RIGHT);
  hildon_helper_set_logical_color(GTK_WIDGET(priv->time_label), GTK_RC_FG,
                                  GTK_STATE_NORMAL, "TitleTextColor");
  hildon_helper_set_logical_color(GTK_WIDGET(priv->time_label), GTK_RC_FG,
                                  GTK_STATE_PRELIGHT, "TitleTextColor");
  priv->alarm_icon_visible = FALSE;
  priv->alarm_on_icon =
      gtk_icon_theme_load_icon(icon_theme, "general_alarm_on", 18, 0, 0);
  priv->status_bar_alarm_image = gtk_image_new();

  priv->apm_label = gtk_label_new("");
  attrs = pango_attr_list_new();
  pango_attr_list_insert(attrs, pango_attr_size_new(11 * PANGO_SCALE));
  pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(priv->apm_label), attrs);
  pango_attr_list_unref(attrs);
  gtk_misc_set_padding(GTK_MISC(priv->apm_label), 0, 0);
  gtk_label_set_justify(GTK_LABEL(priv->apm_label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment(GTK_MISC(priv->apm_label), 0.0, 0.0);
  hildon_helper_set_logical_font(GTK_WIDGET(priv->apm_label), "SystemFont");
  hildon_helper_set_logical_color(GTK_WIDGET(priv->apm_label), GTK_RC_FG,
                                  GTK_STATE_NORMAL, "TitleTextColor");
  hildon_helper_set_logical_color(GTK_WIDGET(priv->apm_label), GTK_RC_FG,
                                  GTK_STATE_PRELIGHT, "TitleTextColor");
  priv->vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(
        GTK_BOX(priv->vbox), priv->status_bar_alarm_image, TRUE, TRUE, 0);

  priv->align1 = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(priv->align1), 14, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(priv->align1), priv->apm_label);
  gtk_box_pack_start(GTK_BOX(priv->vbox), priv->align1, TRUE, TRUE, 0);

  priv->align2 = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(priv->align2), 0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(priv->align2), priv->time_label);
  gtk_box_pack_start(GTK_BOX(priv->hbox), priv->align2, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(priv->hbox), priv->vbox, TRUE, TRUE, 0);
  gtk_widget_show_all(priv->hbox);

  if (priv->time_fmt_24h)
    gtk_widget_hide(GTK_WIDGET(priv->align1));
}

static void
clock_plugin_init(ClockPlugin *plugin)
{
  ClockPluginPrivate *priv;
  GConfClient *gc;
  gboolean time_fmt_24h;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkIconTheme *icon_theme;
  GtkWidget *title;
  char time_fmt[16];
  DBusError error;

  priv = CLOCK_PLUGIN_GET_PRIVATE(plugin);
  plugin->priv = priv;
  dbus_error_init(&error);
  priv->session_bus = dbus_bus_get(DBUS_BUS_SESSION, &error);

  if (dbus_error_is_set(&error))
    dbus_error_free(&error);

  priv->system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

  if (dbus_error_is_set(&error))
    dbus_error_free(&error);

  dbus_bus_add_match(priv->system_bus,
                     "type='signal',sender='com.nokia.clockd',interface='com.nokia.clockd',path='/com/nokia/clockd',member='time_changed'",
                     &error);

  dbus_connection_add_filter(priv->system_bus, _dbus_filter, plugin, NULL);

  priv->osso = osso_initialize("clock_status_area_plugin", "1.0", FALSE, NULL);

  if (priv->osso)
  {
    priv->display_state = OSSO_DISPLAY_ON;
    osso_hw_set_display_event_cb(priv->osso, _handle_signals_cb, plugin);

    if (osso_rpc_set_cb_f(priv->osso, STATUSAREA_CLOCK_SERVICE,
                          STATUSAREA_CLOCK_PATH, STATUSAREA_CLOCK_INTERFACE,
                          _alarms_changed_cb,plugin))
    {
      osso_deinitialize(priv->osso);
      priv->osso = NULL;
    }

    g_signal_connect(G_OBJECT(gdk_screen_get_default()), "size-changed",
                     G_CALLBACK(_cpa_screen_size_changed_cb), priv);
  }

  priv->time_fmt_24h = FALSE;

  gc = gconf_client_get_default();

  g_assert(gc);

  priv->time_fmt_24h = gconf_client_get_bool(gc, "/apps/clock/time-format", 0);

  if (time_get_time_format(time_fmt, sizeof(time_fmt)) >= 0)
  {
    if (g_strcmp0(time_fmt, "%R"))
      time_fmt_24h = FALSE;
    else
    {
      /* g_strcmp0(time_fmt, "%r"); */
      time_fmt_24h = TRUE;
    }

    if (priv->time_fmt_24h != time_fmt_24h)
    {
      priv->time_fmt_24h = time_fmt_24h;
      gconf_client_set_bool(gc, "/apps/clock/time-format", time_fmt_24h, NULL);
    }
  }

  gconf_client_add_dir(gc, "/apps/clock", GCONF_CLIENT_PRELOAD_NONE, NULL);
  priv->gc_notify =
      gconf_client_notify_add(gc, "/apps/clock/time-format",
                              _gc_time_format_changed_cb, plugin, NULL, NULL);

  /* FIXME - "/usr/share/locale", use define */
  bindtextdomain("osso-clock", "/usr/share/locale");
  bindtextdomain("hildon-libs", "/usr/share/locale");

  _create_status_area_widgets(priv);
  hd_status_plugin_item_set_status_area_widget(HD_STATUS_PLUGIN_ITEM(plugin),
                                               GTK_WIDGET(priv->hbox));
  _time_label_style_set_cb(priv->time_label, NULL, plugin);
  g_signal_connect(G_OBJECT(priv->time_label), "style-set",
                   G_CALLBACK(_time_label_style_set_cb), plugin);

  priv->button = hildon_gtk_button_new(HILDON_SIZE_FINGER_HEIGHT);

  g_signal_connect_after(G_OBJECT(priv->button), "clicked",
                         G_CALLBACK(_status_menu_button_clicked_cb), plugin);

  vbox = gtk_vbox_new(TRUE, 0);
  hbox = gtk_hbox_new(FALSE, 8);

  icon_theme = gtk_icon_theme_get_default();
  priv->icon_day_time =
      gtk_icon_theme_load_icon(icon_theme, "clock_day_time", 48, 0, 0);
  priv->icon_night_time =
      gtk_icon_theme_load_icon(icon_theme, "clock_night_time", 48, 0, 0);
  priv->icon_alarm_on =
      gtk_icon_theme_load_icon(icon_theme, "general_alarm_on", 48, 0, 0);

  priv->clock_image = gtk_image_new();
  g_signal_connect_after(G_OBJECT(priv->clock_image), "expose-event",
                         G_CALLBACK(_clock_image_expose_event_cb), plugin);
  gtk_misc_set_alignment(GTK_MISC(priv->clock_image), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(hbox),
                     GTK_WIDGET(priv->clock_image), FALSE, FALSE, 0);

  title = gtk_label_new(dgettext("osso-clock", "dati_ti_menu_plugin"));
  gtk_box_pack_start(GTK_BOX(vbox), title, TRUE, TRUE, 0);

  priv->label3 = gtk_label_new("");

  gtk_box_pack_start(GTK_BOX(vbox), priv->label3, TRUE,TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(priv->button), hbox);
  gtk_misc_set_alignment(GTK_MISC(title), 0.0, 1.0);
  gtk_misc_set_alignment(GTK_MISC(priv->label3), 0.0, 1.0);

  hildon_helper_set_logical_color(GTK_WIDGET(priv->label3), GTK_RC_FG,
                                  GTK_STATE_NORMAL, "SecondaryTextColor");
  hildon_helper_set_logical_color(GTK_WIDGET(priv->label3), GTK_RC_FG,
                                  GTK_STATE_PRELIGHT, "SecondaryTextColor");
  hildon_helper_set_logical_font(GTK_WIDGET(priv->label3), "SmallSystemFont");
  gtk_container_add(GTK_CONTAINER(plugin), priv->button);
  gtk_widget_show_all(GTK_WIDGET(plugin));
  _show_status_menu_clock(plugin);
  time_get_synced();
}
