#!/bin/sh

export AG_APPLICATIONS=$TESTDATADIR
export AG_SERVICES=$TESTDATADIR
export AG_SERVICE_TYPES=$TESTDATADIR
export AG_PROVIDERS=$TESTDATADIR
export ACCOUNTS=/tmp/
export AG_DEBUG=all
export G_MESSAGES_DEBUG=all
export G_DEBUG=fatal-criticals
# If running the test executable under a wrapper, setup the tests so that the
# wrapper can debug them more easily.
if [ -n "$WRAPPER" ]; then
    export G_SLICE=always-malloc
    export CK_FORK="no"
else
    export G_SLICE=debug-blocks
fi
export XDG_DATA_HOME=$TESTDATADIR
export PATH=.:$PATH

# If dbus-test-runner exists, use it to run the tests in a separate D-Bus
# session
if command -v dbus-test-runner > /dev/null ; then
    echo "Using dbus-test-runner"
    dbus-test-runner -m 360 -t "$TESTDIR"/accounts-glib-test-wrapper.sh
else
    echo "Using existing D-Bus session"
    "$TESTDIR"/accounts-glib-test-wrapper.sh
fi
