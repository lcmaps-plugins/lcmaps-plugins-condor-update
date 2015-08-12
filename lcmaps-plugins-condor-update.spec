Summary: Plugin for updating a Condor ClassAd from LCMAPS authorization framework
Name: lcmaps-plugins-condor-update
Version: 0.2.1
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

BuildRequires: lcmaps-common-devel

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
mv $RPM_BUILD_ROOT/%{_libdir}/lcmaps/liblcmaps_condor_update.so $RPM_BUILD_ROOT/%{_libdir}/lcmaps/lcmaps_condor_update.mod
rm $RPM_BUILD_ROOT/%{_libdir}/lcmaps/liblcmaps_condor_update.la
rm $RPM_BUILD_ROOT/%{_libdir}/lcmaps/liblcmaps_condor_update.a

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libdir}/lcmaps/lcmaps_condor_update.mod

%changelog
* Wed Aug 12 2015 Brian Bockelman <bbockelm@cse.unl.edu> - 0.2.1-1
- Fix usage of _CONDOR_CHIRP_CONFIG environment variable.

* Mon Aug 10 2015 Brian Bockelman <bbockelm@cse.unl.edu> - 0.2.0-1
- Fix support for PID namespaces.

* Sun Jan 08 2012 Brian Bockelman <bbockelm@cse.unl.edu> - 0.1.0-1
- Update to reflect the new LCMAPS modules directory.

* Fri Sep 30 2011 Brian Bockelman <bbockelm@cse.unl.edu> - 0.0.2-1
- Fix memory leaks and track the correct PID.

* Fri Sep 23 2011 Brian Bockelman <bbockelm@cse.unl.edu> - 0.0.1-1
- Initial build of the process tracking plugin.

