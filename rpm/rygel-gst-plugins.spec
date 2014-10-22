Name:          rygel-gst-plugins
Version:       0.20.0
Release:       1%{?dist}
Summary:       A collection of rygel plugins

Group:         Applications/Multimedia
License:       LGPLv2+
URL:           http://live.gnome.org/Rygel
Source0:       %{name}-%{version}.tar.xz

BuildRequires: gnome-common
BuildRequires: gobject-introspection-devel >= 1.36
BuildRequires: dbus-glib-devel
BuildRequires: desktop-file-utils
BuildRequires: pkgconfig(gstreamer-0.10)
BuildRequires: pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires: pkgconfig(rygel-core-2.4)
BuildRequires: gupnp-devel
BuildRequires: gupnp-av-devel
BuildRequires: gupnp-dlna-devel
BuildRequires: libgee-devel
BuildRequires: libsoup-devel
#BuildRequires: libunistring-devel
BuildRequires: libuuid-devel
BuildRequires: sqlite-devel
BuildRequires: tracker-devel
BuildRequires: pkgconfig(libmediaart-1.0)
BuildRequires: intltool

%description
Rygel is a home media solution that allows you to easily share audio, video and
pictures, and control of media player on your home network. In technical terms
it is both a UPnP AV MediaServer and MediaRenderer implemented through a plug-in
mechanism. Interoperability with other devices in the market is achieved by
conformance to very strict requirements of DLNA and on the fly conversion of
media to format that client devices are capable of handling.

%package devel
Summary: Development package for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
Files for development with %{name}.

%prep
%setup -q -n %{name}-%{version}/rygel-gst-0-10-plugins

%build
%autogen

make %{?_smp_mflags} V=1

%install
make install DESTDIR=%{buildroot} INSTALL='install -p'

#Remove libtool archives.
find %{buildroot} -name '*.la' -exec rm -f {} ';'

%find_lang %{name}

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files -f %{name}.lang
%{_libdir}/rygel-2.4/plugins/*

%files devel
%{_datadir}/rygel-2.4/*
