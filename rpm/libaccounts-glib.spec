Name: libaccounts-glib
Version: 0.39
Release: 1%{?dist}
Group: System Environment/Libraries
Summary: Nokia Maemo Accounts base library
License: LGPLv2.1
URL: http://gitorious.org/accounts-sso/accounts-glib
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: automake
BuildRequires: glib2-devel
BuildRequires: pkgconfig
BuildRequires: gtk-doc
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(sqlite3)
BuildRequires: pkgconfig(dbus-1)
BuildRequires: pkgconfig(dbus-glib-1)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(check)  >= 0.9.4

%description
%{summary}.

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_libdir}/lib*.so.*
%{_datadir}/backup-framework/applications/*.conf
#exclude tests for now
%exclude %{_bindir}/*test*
%exclude %{_datadir}/libaccounts-glib0-test
%exclude /usr/doc/reference

%package devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: glib2-devel
Requires: pkgconfig

%description devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%files devel
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_libdir}/*.la
%{_libdir}/pkgconfig/*
%{_includedir}/*

%prep
%setup -q

%build
gtkdocize
autoreconf -i --force

%configure \
        --disable-static \
        --disable-gtk-doc

make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}


%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig
