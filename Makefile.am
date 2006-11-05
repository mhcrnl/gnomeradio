SUBDIRS = help data po src

distuninstallcheck_listfiles = find . -type f -print | grep -v scrollkeeper

EXTRA_DIST = intltool-update.in \
             intltool-merge.in \
             intltool-extract.in \
             README \
             README.lirc \
             README.recording \
             gnomeradio.spec.in \
             gnomeradio.spec \
             gnome-doc-utils.make

CLEANFILES = gnomeradio.spec

DISTCLEANFILES = gnome-doc-utils.make \
                 intltool-extract \
                 intltool-merge \
                 intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper
