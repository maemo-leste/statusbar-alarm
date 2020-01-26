hildondesktoplib_LTLIBRARIES = clock-plugin.la

clock_plugin_la_CFLAGS = -Wall -Werror $(LIBHILDON_CFLAGS) $(HILDON_CFLAGS) \
			 $(OSSO_CFLAGS) $(GCONF_CFLAGS) -DOSSOLOG_COMPILE=1 \
			 $(STATUSBAR_ALARM_CFLAGS) $(TIME_CFLAGS) \
			 -DG_LOG_DOMAIN=\"$(PACKAGE)\" \
			 -DLOCALE_DIR=\"$(localedir)\"

clock_plugin_la_LDFLAGS = -Wl,--as-needed -lm $(LIBHILDON_LIBS) $(HILDON_LIBS) \
			  $(OSSO_LIBS) $(STATUSBAR_ALARM_LIBS) $(GCONF_LIBS) \
			  $(TIME_LIBS) -module -avoid-version -Wl, no-undefined

clock_plugin_la_SOURCES = clock-plugin.c

hildonstatusmenudesktopentry_DATA = clock-plugin.desktop
EXTRA_DIST = $(hildonstatusmenudesktopentry_DATA)

MAINTAINERCLEANFILES = Makefile.in

install-data-hook:
	@(cd $(DESTDIR)$(hildondesktoplibdir) && $(RM) \
		$(hildondesktoplib_LTLIBRARIES))
