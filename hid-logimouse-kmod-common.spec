%define debug_package %{nil}

Name: hid-logimouse-kmod-common

Version:        0.2.0
Release:        1%{?dist}.1
Summary:        Logi mouse HID kernel module

Group:          System Environment/Kernel

License:        GPL-2.0
URL:            https://github.com/kyokenn/hid-logi-mouse
Source0:        hid-logimouse_%{version}.orig.tar.xz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires:       kmod-hid-logimouse


%description
Kernel module for Logitech mice


%prep
%autosetup -n hid-logimouse_%{version}


%setup -q -c -T -a 0


%build


%install


%clean
rm -rf ${RPM_BUILD_ROOT}


%pre


%files
