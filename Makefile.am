SUBDIRS = docs pixmaps po src

Applicationsdir = $(datadir)/applications/
applications_in_files = gnomeradio.desktop.in
Applications_DATA = $(applications_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gnomeradio.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

spec = $(PACKAGE).spec

all-local: $(spec) $(Applications_DATA)

install-data-local: 
if INSTALL_SCHEMAS
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)
endif

EXTRA_DIST = $(Applications_DATA) \
             $(applications_in_files) \
             $(schema_DATA) \
             $(schema_in_files) \
             $(spec) \
             intltool-update.in \
             intltool-merge.in \
             intltool-extract.in \
             README.lirc \
             README.recording \
             example.lircrc \
	     xmldocs.make \
	     omf.make
