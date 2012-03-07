#!/bin/sh

export AG_APPLICATIONS=$TESTDATADIR
export AG_SERVICES=$TESTDATADIR
export AG_SERVICE_TYPES=$TESTDATADIR
export AG_PROVIDERS=$TESTDATADIR
export ACCOUNTS=/tmp/
export AG_DEBUG=all
export G_MESSAGES_DEBUG=all

accounts-glib-testsuite "$@"
