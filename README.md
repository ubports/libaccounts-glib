Accounts management library for GLib applications
=================================================

This project is a library for managing accounts which can be used from GLib
applications. It is part of the @accounts-sso project.


License
-------

See COPYING file.


Build instructions
------------------

The project depends on GLib (including GIO and GObject), libxml2, sqlite3 and
check.
To build it, run:
```
./autogen.sh
./configure
make
make install
```
