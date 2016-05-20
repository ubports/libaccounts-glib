Accounts management library for GLib applications
=================================================

This project is a library for managing accounts which can be used from GLib
applications. It is part of the [accounts-sso project][accounts-sso].


License
-------

See COPYING file.


Build instructions
------------------

The project depends on GLib (including GIO and GObject), libxml2, sqlite3 and
[check][].
To build it, run:
```
./autogen.sh
./configure
make
make install
```

Resources
---------

[API reference documentation](http://accounts-sso.gitlab.io/libaccounts-glib/)

[Official source code repository](https://gitlab.com/accounts-sso/libaccounts-glib)

[accounts-sso]: https://gitlab.com/groups/accounts-sso
[check]: https://github.com/libcheck/check
