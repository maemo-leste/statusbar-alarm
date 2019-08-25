hildondesktoplib_LTLIBRARIES = clock-plugin.la

AM_CFLAGS = -Wall -Werror $(LIBHILDON_CFLAGS) $(HILDON_CFLAGS) $(OSSO_CFLAGS) \
	     $(GCONF_CFLAGS) $(STATUSBAR_ALARM_CFLAGS) $(TIME_CFLAGS) \
	     -DG_LOG_DOMAIN=\"$(PACKAGE)\" -DOSSOLOG_COMPILE=1

AM_LDFLAGS = -Wl,--as-needed $(LIBHILDON_LIBS) $(HILDON_LIBS) $(OSSO_LIBS) \
	     $(STATUSBAR_ALARM_LIBS) $(GCONF_LIBS) $(TIME_LIBS) \
	     -lm -module -avoid-version -Wl, no-undefined

clock_plugin_la_SOURCES = clock-plugin.c

hildonstatusmenudesktopentry_DATA = clock-plugin.desktop
EXTRA_DIST = $(hildonstatusmenudesktopentry_DATA)

MAINTAINERCLEANFILES = Makefile.in
