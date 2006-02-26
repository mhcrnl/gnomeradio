INCLUDES = -I$(top_srcdir) -I$(includedir) \
           $(GNOME_CFLAGS) $(GSTREAMER_CFLAGS) \
           -DG_LOG_DOMAIN=\"gnomeradio\" \
           -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
           -DDATADIR=\""$(datadir)/"\" \
           -DPREFIX=\""$(prefix)/"\" \
           -DLIBDIR=\""$(libdir)/"\" \
           -DSYSCONFDIR=\""$(sysconfdir)/"\"


bin_PROGRAMS = gnomeradio

gnomeradio_SOURCES = eggtrayicon.c bacon-volume.c trayicon.c gui.c tech.c prefs.c rec_tech.c record.c lirc.c \
                     eggtrayicon.h bacon-volume.h trayicon.h gui.h tech.h prefs.h rec_tech.h record.h lirc.h

gnomeradio_LDADD = $(GNOME_LIBS) $(GSTREAMER_LIBS) $(LIRC)
