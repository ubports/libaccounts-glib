SUBDIRS = libaccounts-glib docs tests tools

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libaccounts-glib.pc

libaccountsdocdir = ${prefix}/doc/reference
libaccountsdoc_DATA= README COPYING AUTHORS ChangeLog INSTALL NEWS

backupconfdir = ${prefix}/share/backup-framework/applications
backupconf_in_files = accounts.conf.in 
backupconf_DATA= accounts.conf

EXTRA_DIST = $(libaccountsdoc_DATA) $(backupconf_DATA)

$(backupconf_DATA): $(backupconf_in_files) 
	@sed -e "s|\@DATABASE_DIR\@|$(DATABASE_DIR)|" $< > $@ 
