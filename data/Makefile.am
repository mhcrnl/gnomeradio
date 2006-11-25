SUBDIRS = icons pixmaps

applicationsdir = $(datadir)/applications/
applications_in_files = gnomeradio.desktop.in
applications_DATA = $(applications_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gnomeradio.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

all-local: $(applications_DATA)

install-data-local: 
if INSTALL_SCHEMAS
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)
endif

EXTRA_DIST = $(applications_DATA) \
             $(applications_in_files) \
             $(schema_DATA) \
             $(schema_in_files) \
             example.lircrc

CLEANFILES = $(schema_DATA) $(applications_DATA)
