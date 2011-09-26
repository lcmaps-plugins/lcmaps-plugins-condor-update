Summary: Plugin for updating a Condor ClassAd from LCMAPS authorization framework
Name: lcmaps-plugins-condor-update
Version: 0.0.1
Release: 1%{?dist}
License: Public Domain
Group: System Environment/Libraries
# The tarball was created from Subversion using the following commands:
# svn co svn://t2.unl.edu/brian/lcmaps-plugin-condor-update
# cd lcmaps-plugin-process-tracking
# ./bootstrap
# ./configure
# make dist
Source0: %{name}-%{version}.tar.gz

BuildRequires: lcmaps-interface

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
This plugin updates the running job's Condor ClassAd with information
about the glexec target.

%prep
%setup -q

%build

%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT install
mv $RPM_BUILD_ROOT/%{_libdir}/lcmaps $RPM_BUILD_ROOT/%{_libdir}/modules
ln -s liblcmaps_condor_update.so $RPM_BUILD_ROOT/%{_libdir}/modules/liblcmaps_condor_update.so.0
ln -s liblcmaps_condor_update.so.0 $RPM_BUILD_ROOT/%{_libdir}/modules/liblcmaps_condor_update.so.0.0.0
rm $RPM_BUILD_ROOT/%{_libdir}/modules/liblcmaps_condor_update.la
rm $RPM_BUILD_ROOT/%{_libdir}/modules/liblcmaps_condor_update.a

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libdir}/modules/lcmaps_condor_update.mod
%{_libdir}/modules/liblcmaps_condor_update.so
%{_libdir}/modules/liblcmaps_condor_update.so.0
%{_libdir}/modules/liblcmaps_condor_update.so.0.0.0

%changelog
* Fri Sep 23 2011 Brian Bockelman <bbockelm@cse.unl.edu> - 0.0.1-1
- Initial build of the process tracking plugin.

