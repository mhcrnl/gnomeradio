INCLUDES = -I$(top_srcdir) -I$(includedir) \
           $(GNOMERADIO_CFLAGS) \
           -DG_LOG_DOMAIN=\"gnomeradio\" \
           -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
           -DGNOME_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED \
           -DGDK_DISABLE_DEPRECATED -DG_DISABLE_DEPRECATED \
           -DDATADIR=\""$(datadir)/"\" \
           -DPREFIX=\""$(prefix)/"\" \
           -DLIBDIR=\""$(libdir)/"\" \
           -DSYSCONFDIR=\""$(sysconfdir)/"\"


bin_PROGRAMS = gnomeradio

gnomeradio_SOURCES = gui.c tech.c prefs.c rec_tech.c record.c lirc.c gui.h tech.h prefs.h rec_tech.h record.h lirc.h           
		
gnomeradio_LDADD = $(GNOMERADIO_LIBS) $(LIRC)
