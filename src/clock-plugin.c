#include <clock-plugin-dbus.h>
#include <time.h>
#include <clockd/libtime.h>
#include <gconf/gconf-client.h>
#include <libhildondesktop/libhildondesktop.h>
#include <libosso.h>

#include <libintl.h>
#include <string.h>

#include "config.h"

#define _(x) dgettext("hildon-libs", x)

#define CLOCK_ICON_SIZE_AREA 18
#define _TIME_CLOCK_FONT 21

#define CLOCK_PLUGIN_TYPE (clock_plugin_get_type())
#define CLOCK_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLOCK_PLUGIN_TYPE, ClockPlugin))

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
_alarms_changed(const gchar *interface, const gchar *method, GArray *arguments,
                gpointer data, osso_rpc_t *retval)
{
  if (!g_ascii_strcasecmp(method, STATUSAREA_CLOCK_ALARM_SET))
    _show_status_menu_widget(data, TRUE);
  else if (!g_ascii_strcasecmp(method, STATUSAREA_CLOCK_NO_ALARMS))
    _show_status_menu_widget(data, FALSE);

  return 0;
}

static gboolean
_idle_update_time(gpointer user_data)
{
  static struct tm last_update_time;
  ClockPlugin *plugin = user_data;
  ClockPluginPrivate *priv = CLOCK_PLUGIN(plugin)->priv;

  time_get_local(&last_update_time);

  priv->time_changed_id =
      gdk_threads_add_timeout(1000 * (60 - last_update_time.tm_sec) + 500,
                            _idle_update_time, plugin);

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
        priv->time_changed_id = gdk_threads_add_idle(_idle_update_time, plugin);
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
                        _alarms_changed, object);
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
clock_plugin_class_init(ClockPluginClass *klass)
{
  G_OBJECT_CLASS(klass)->dispose = _clock_plugin_dispose;

  g_type_class_add_private(klass, sizeof(ClockPluginPrivate));
}

static void
clock_plugin_init(ClockPlugin *plugin)
{

}
