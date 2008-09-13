INCLUDES = -I$(top_srcdir) -I$(includedir) \
           $(GNOME_CFLAGS) $(GSTREAMER_CFLAGS) \
           -DG_LOG_DOMAIN=\"gnomeradio\" \
           -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
           -DDATADIR=\""$(datadir)/"\" \
           -DPREFIX=\""$(prefix)/"\" \
           -DLIBDIR=\""$(libdir)/"\" \
           -DSYSCONFDIR=\""$(sysconfdir)/"\"


bin_PROGRAMS = gnomeradio

gnomeradio_SOURCES = \
            bacon-volume.h \
            gui.h \
            lirc.h \
            prefs.h \
            radio.h \
            rec_tech.h \
            record.h \
            tech.h \
            trayicon.h \
            v4l1.h \
            v4l2.h \
            bacon-volume.c \
            gui.c \
            lirc.c \
            prefs.c \
            radio.c \
            rec_tech.c \
            record.c \
            tech.c \
            trayicon.c \
            v4l1.c \
            v4l2.c

gnomeradio_LDADD = $(GNOME_LIBS) $(GSTREAMER_LIBS) $(LIRC)
