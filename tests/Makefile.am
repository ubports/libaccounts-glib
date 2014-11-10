## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)

test_PROGRAMS = \
	accounts-glib-testsuite \
	test-process

check_PROGRAMS = \
	accounts-glib-testsuite \
	test-process

if HAVE_XMLLINT
xml_test = xml-dtd-validate.sh
else
xml_test =
endif

test_SCRIPTS = \
	accounts-glib-test.sh

dist_test_SCRIPTS = \
	accounts-glib-test-wrapper.sh

dist_check_SCRIPTS = \
	accounts-glib-test.sh \
	$(xml_test)

accounts_glib_testsuite_SOURCES = check_ag.c
nodist_accounts_glib_testsuite_SOURCES = \
	test-manager.c \
	test-manager.h
accounts_glib_testsuite_CPPFLAGS = \
	$(CHECK_CFLAGS) \
	-I$(top_builddir) \
	-I$(top_builddir)/tests \
	$(AM_CPPFLAGS)
accounts_glib_testsuite_LDADD = \
	$(CHECK_LIBS) \
	$(top_builddir)/libaccounts-glib/libaccounts-glib.la

test_process_SOURCES = test-process.c
test_process_CPPFLAGS = \
	$(LIBACCOUNTS_CFLAGS) \
	$(AM_CPPFLAGS)
test_process_LDADD = \
	$(LIBACCOUNTS_LIBS) \
	$(top_builddir)/libaccounts-glib/libaccounts-glib.la

BUILT_SOURCES = \
	$(nodist_accounts_glib_testsuite_SOURCES)

TESTS_ENVIRONMENT = \
	DTDDIR=$(top_srcdir)/libaccounts-glib/; export DTDDIR; \
	TESTDATADIR=$(top_srcdir)/tests/; export TESTDATADIR; \
	TESTDIR=$(top_srcdir)/tests/; export TESTDIR; \
	XMLLINT=$(XMLLINT); export XMLLINT; \
	abs_top_builddir=$(abs_top_builddir); export abs_top_builddir;

TESTS = $(dist_check_SCRIPTS)

tests.xml: tests.xml.in Makefile
	$(SED) -e 's|@''TESTDIR''@|$(testdir)|' \
	       -e 's|@''TESTDATADIR''@|$(testdatadir)|' $< > $@
CLEANFILES = \
	$(BUILT_SOURCES) \
	tests.xml

dist_noinst_DATA = \
	tests.xml.in

test_DATA = \
	tests.xml

dist_testdata_DATA = \
	e-mail.service-type \
	Gallery.application \
	Mailer.application \
	MyProvider.provider \
	MyService.service \
	MyService2.service \
	OtherService.service \
	maemo.provider

nobase_dist_testdata_DATA = \
	applications/Mailer.desktop

installcheck-local: $(TESTS)
	cd "$(testdir)" && TESTDATADIR="$(testdatadir)" ./accounts-glib-test.sh

test-manager.h test-manager.c: $(top_srcdir)/libaccounts-glib/com.google.code.AccountsSSO.Accounts.Manager.xml
	$(AM_V_GEN)gdbus-codegen \
		--generate-c-code test-manager \
		--c-generate-object-manager \
		--c-namespace Test \
		--annotate "com.google.code.AccountsSSO.Accounts.Manager" org.gtk.GDBus.C.Name Manager \
		$<
