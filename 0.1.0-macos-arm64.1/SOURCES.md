# Sources of the Koala runtime `0.1.0-macos-arm64.1`

Built from Koala commit `6cfca7d7485f23c523676c2a8e4ad147773d621c`. Every file below is in the
release `0.1.0-macos-arm64.1` of this repository, in the part named in its path
(`kernel/...` is in `kernel.tar`), with its SHA-256 in `manifest.json`.

## Linux kernels

| Kernel | Licence | Version | Source | Build configuration |
| --- | --- | --- | --- | --- |
| `full/vmlinux-arm64` | GPL-2.0-only | Linux 6.18.60 | `kernel/linux-6.18.60.tar.xz` | `kernel/config-full-arm64` |
| `lean/vmlinux.container` | GPL-2.0-only | Linux 6.18.60 | `kernel/linux-6.18.60.tar.xz` | `kernel/config-lean-arm64` |
| `lean/init.ext4: /sbin/vminitd, /sbin/vmexec` | Apache-2.0 | | https://github.com/apple/containerization (Apple's vminit image, by digest; statically linked with musl, MIT) (not GPL; not copied) | |

Each kernel's build configuration comes from rebuilding it with the scripts and
fragments in `kernel/recipe/`; the rebuild is byte-identical to the shipped kernel.

## Ubuntu packages (from Launchpad, at the installed versions)

| Package | Version | Licence | Source package |
| --- | --- | --- | --- |
| adduser | 3.137ubuntu1 | GPL-2+ | `ubuntu/adduser_3.137ubuntu1.dsc` |
| apt | 2.8.3 | GPL-2+, GPL-2, BSD-3-clause, Expat | `ubuntu/apt_2.8.3.dsc` |
| base-files | 13ubuntu10.5 | see-copyright | `ubuntu/base-files_13ubuntu10.5.dsc` |
| base-passwd | 3.6.3build1 | GPL-2, public-domain | `ubuntu/base-passwd_3.6.3build1.dsc` |
| bash | 5.2.21-2ubuntu4 | GPL-3+, GPL-3+ with Bison exception, GPL-2+, GFDL-NIV-1.3, Latex2e, BSD-4-clause-UC and MIT-like, BSD-4-clause-UC, MIT-like, permissive | `ubuntu/bash_5.2.21-2ubuntu4.dsc` |
| bind9-dnsutils | 1:9.18.39-0ubuntu0.24.04.7 | MPL-2.0 and ISC and BSD-2-clause and BSD-3-clause, MPL-2.0 and ISC, MPL-2.0 and CC0-1.0, MPL-2.0 and public-domain, FSFAP, ISC or MPL-2.0, MPL-2.0, ISC, BSD-2-clause, BSD-3-clause, CC0-1.0 | `ubuntu/bind9_9.18.39-0ubuntu0.24.04.7.dsc` |
| bind9-host | 1:9.18.39-0ubuntu0.24.04.7 | MPL-2.0 and ISC and BSD-2-clause and BSD-3-clause, MPL-2.0 and ISC, MPL-2.0 and CC0-1.0, MPL-2.0 and public-domain, FSFAP, ISC or MPL-2.0, MPL-2.0, ISC, BSD-2-clause, BSD-3-clause, CC0-1.0 | `ubuntu/bind9_9.18.39-0ubuntu0.24.04.7.dsc` |
| bind9-libs | 1:9.18.39-0ubuntu0.24.04.7 | MPL-2.0 and ISC and BSD-2-clause and BSD-3-clause, MPL-2.0 and ISC, MPL-2.0 and CC0-1.0, MPL-2.0 and public-domain, FSFAP, ISC or MPL-2.0, MPL-2.0, ISC, BSD-2-clause, BSD-3-clause, CC0-1.0 | `ubuntu/bind9_9.18.39-0ubuntu0.24.04.7.dsc` |
| bridge-utils | 1.7.1-1ubuntu2 | see-copyright | `ubuntu/bridge-utils_1.7.1-1ubuntu2.dsc` |
| bsdutils | 1:2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| busybox-static (guest tools) | 1:1.36.1-6ubuntu3.1 | see the source's debian/copyright | `ubuntu/busybox_1.36.1-6ubuntu3.1.dsc` |
| ca-certificates | 20260601~24.04.1 | GPL-2+, MPL-2.0 | `ubuntu/ca-certificates_20260601~24.04.1.dsc` |
| containerd.io | 2.3.6-1~ubuntu.24.04~noble | Apache-2.0 | Docker's repository, not copied |
| coreutils | 9.4-3ubuntu6.3 | GPL-3+, GPL-3+ and BSD-4-clause-UC, BSD-4-clause-UC, GPL-3+ and ISC, ISC, FSFULLR, GFDL-NIV-1.3 | `ubuntu/coreutils_9.4-3ubuntu6.3.dsc` |
| curl | 8.5.0-2ubuntu10.15 | curl, OLDAP-2.8, ISC, GPL-2+ with Autoconf-data exception, GPL-3+ with Autoconf-data exception, GPL-2+ with Libtool exception, BSD-3-clause, BSD-4-Clause-UC, FSFULLR, X11, BSD-3-Clause | `ubuntu/curl_8.5.0-2ubuntu10.15.dsc` |
| dash | 0.5.12-6ubuntu5 | BSD-3-Clause, public-domain, GPL-2+, BSD-3-clause | `ubuntu/dash_0.5.12-6ubuntu5.dsc` |
| dbus | 1.14.10-4ubuntu4.1 | GPL-2+ or AFL-2.1, and Expat and Tcl-BSDish, GPL-2+, BSD-3-clause and BSD-3-clause-generic, autoconf-archive-permissive, FSF-unlimited-permission, LGPL-2.1+, g10-permissive, Expat, Tcl-BSDish, BSD-3-clause, BSD-3-clause-generic, AFL-2.1 | `ubuntu/dbus_1.14.10-4ubuntu4.1.dsc` |
| dbus-bin | 1.14.10-4ubuntu4.1 | GPL-2+ or AFL-2.1, and Expat and Tcl-BSDish, GPL-2+, BSD-3-clause and BSD-3-clause-generic, autoconf-archive-permissive, FSF-unlimited-permission, LGPL-2.1+, g10-permissive, Expat, Tcl-BSDish, BSD-3-clause, BSD-3-clause-generic, AFL-2.1 | `ubuntu/dbus_1.14.10-4ubuntu4.1.dsc` |
| dbus-daemon | 1.14.10-4ubuntu4.1 | GPL-2+ or AFL-2.1, and Expat and Tcl-BSDish, GPL-2+, BSD-3-clause and BSD-3-clause-generic, autoconf-archive-permissive, FSF-unlimited-permission, LGPL-2.1+, g10-permissive, Expat, Tcl-BSDish, BSD-3-clause, BSD-3-clause-generic, AFL-2.1 | `ubuntu/dbus_1.14.10-4ubuntu4.1.dsc` |
| dbus-session-bus-common | 1.14.10-4ubuntu4.1 | GPL-2+ or AFL-2.1, and Expat and Tcl-BSDish, GPL-2+, BSD-3-clause and BSD-3-clause-generic, autoconf-archive-permissive, FSF-unlimited-permission, LGPL-2.1+, g10-permissive, Expat, Tcl-BSDish, BSD-3-clause, BSD-3-clause-generic, AFL-2.1 | `ubuntu/dbus_1.14.10-4ubuntu4.1.dsc` |
| dbus-system-bus-common | 1.14.10-4ubuntu4.1 | GPL-2+ or AFL-2.1, and Expat and Tcl-BSDish, GPL-2+, BSD-3-clause and BSD-3-clause-generic, autoconf-archive-permissive, FSF-unlimited-permission, LGPL-2.1+, g10-permissive, Expat, Tcl-BSDish, BSD-3-clause, BSD-3-clause-generic, AFL-2.1 | `ubuntu/dbus_1.14.10-4ubuntu4.1.dsc` |
| dbus-user-session | 1.14.10-4ubuntu4.1 | GPL-2+ or AFL-2.1, and Expat and Tcl-BSDish, GPL-2+, BSD-3-clause and BSD-3-clause-generic, autoconf-archive-permissive, FSF-unlimited-permission, LGPL-2.1+, g10-permissive, Expat, Tcl-BSDish, BSD-3-clause, BSD-3-clause-generic, AFL-2.1 | `ubuntu/dbus_1.14.10-4ubuntu4.1.dsc` |
| debconf | 1.5.86ubuntu1 | BSD-2-clause | `ubuntu/debconf_1.5.86ubuntu1.dsc` |
| debianutils | 5.17build1 | GPL-2+, public-domain, SMAIL-GPL | `ubuntu/debianutils_5.17build1.dsc` |
| diffutils | 1:3.10-1ubuntu0.1 | GPL-3+, FSFULLR, GPL-3+ and FSFULLR, LGPL-2.1+, GPL-3+ with autoconf exception, GPL-3+ with texinfo exception, LGPL-2.0+, GPL-2+, X11, FSFAP, GFDL-NIV-1.3, LGPL-3.0+, LGPL-3.0+ or GPL-2+, public-domain | `ubuntu/diffutils_3.10-1ubuntu0.1.dsc` |
| dirmngr | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| dnsutils | 1:9.18.39-0ubuntu0.24.04.7 | MPL-2.0 and ISC and BSD-2-clause and BSD-3-clause, MPL-2.0 and ISC, MPL-2.0 and CC0-1.0, MPL-2.0 and public-domain, FSFAP, ISC or MPL-2.0, MPL-2.0, ISC, BSD-2-clause, BSD-3-clause, CC0-1.0 | `ubuntu/bind9_9.18.39-0ubuntu0.24.04.7.dsc` |
| docker-buildx-plugin | 0.37.1-1~ubuntu.24.04~noble | Apache-2.0 | Docker's repository, not copied |
| docker-ce | 5:29.8.2-1~ubuntu.24.04~noble | Apache-2.0 | Docker's repository, not copied |
| docker-ce-cli | 5:29.8.2-1~ubuntu.24.04~noble | Apache-2.0 | Docker's repository, not copied |
| dpkg | 1.22.6ubuntu6.6 | GPL-2+, public-domain-s-s-d | `ubuntu/dpkg_1.22.6ubuntu6.6.dsc` |
| e2fsprogs | 1.47.0-2.4~exp1ubuntu4.1 | GPL-2, LGPL-2, BSD-3-Clause, Apache-2, ISC, GPL or MIT-US-export, Kazlib, Latex2e, GPL-2+ with Texinfo exception | `ubuntu/e2fsprogs_1.47.0-2.4~exp1ubuntu4.1.dsc` |
| findutils | 4.9.0-5build1 | GFDL-NIV-1.3+, GPL-3+, FSFAP, GPL-3+ with Autoconf-data exception, FSFULLR, GPL-2+ with Autoconf-data exception, GPL-2+, X11, public-domain, GPL with automake exception, LGPL-2.1+, LGPL-2+, LGPL-3+, BSD-3-clause and/or GPL-3+, GPL-3+ with Bison-2.2 exception, LGPL-3, ISC and/or LGPL-2.1+, BSD-3-clause, ISC | `ubuntu/findutils_4.9.0-5build1.dsc` |
| gawk | 1:5.2.1-2ubuntu0.1 | GPL-3+ | `ubuntu/gawk_5.2.1-2ubuntu0.1.dsc` |
| gcc-14-base | 14.2.0-4ubuntu2~24.04.1 | see-copyright | `ubuntu/gcc-14_14.2.0-4ubuntu2~24.04.1.dsc` |
| gnupg | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| gnupg-utils | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| gpg | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| gpg-agent | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| gpgconf | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| gpgsm | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| gpgv | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| grep | 3.11-4build1 | GPL-3+ | `ubuntu/grep_3.11-4build1.dsc` |
| gzip | 1.12-1ubuntu3.2 | GPL-3+, GFDL-1.3+-no-invariant, FSF-manpages | `ubuntu/gzip_1.12-1ubuntu3.2.dsc` |
| hostname | 3.23+nmu2ubuntu2 | GPL-2 | `ubuntu/hostname_3.23+nmu2ubuntu2.dsc` |
| init-system-helpers | 1.66ubuntu1 | BSD-3-clause, GPL-2+ | `ubuntu/init-system-helpers_1.66ubuntu1.dsc` |
| iproute2 | 6.1.0-1ubuntu6.4 | GPL-2 | `ubuntu/iproute2_6.1.0-1ubuntu6.4.dsc` |
| iptables | 1.8.10-3ubuntu2 | GPL-2, Artistic, GPL-2+, custom | `ubuntu/iptables_1.8.10-3ubuntu2.dsc` |
| jq | 1.7.1-3ubuntu0.24.04.2 | MIT, CC-BY-3.0, Expat, GPL-2.0+ | `ubuntu/jq_1.7.1-3ubuntu0.24.04.2.dsc` |
| keyboxd | 2.4.4-2ubuntu17.6 | GPL-3+, permissive, LGPL-2.1+, LGPL-3+ or GPL-2+, Expat, GPL-3+ or BSD-3-clause, LGPL-3+, RFC-Reference, TinySCHEME, CC0-1.0, BSD-3-clause, GPL-2+ | `ubuntu/gnupg2_2.4.4-2ubuntu17.6.dsc` |
| kmod | 31+20240202-2ubuntu7.2 | LGPL-2.1+, GPL-2+ | `ubuntu/kmod_31+20240202-2ubuntu7.2.dsc` |
| libacl1 | 2.3.2-1build1.1 | GPL-2+, LGPL-2+ | `ubuntu/acl_2.3.2-1build1.1.dsc` |
| libapparmor1 | 4.0.1really4.0.1-0ubuntu0.24.04.8 | GPL-2+, BSD-3-clause or GPL-2+, LGPL-2.1+, GPL-2, BSD-3-clause | `ubuntu/apparmor_4.0.1really4.0.1-0ubuntu0.24.04.8.dsc` |
| libapt-pkg6.0t64 | 2.8.3 | GPL-2+, GPL-2, BSD-3-clause, Expat | `ubuntu/apt_2.8.3.dsc` |
| libargon2-1 | 0~20190702+dfsg-4build1 | CC0 or Apache-2.0, CC0, Apache-2.0 | `ubuntu/argon2_0~20190702+dfsg-4build1.dsc` |
| libassuan0 | 2.5.6-1build1 | LGPL-2.1+, GAP~FSF, LGPL-3+, GPL-2+ with libtool exception, GPL-3+, GAP, GPL-2+ | `ubuntu/libassuan_2.5.6-1build1.dsc` |
| libattr1 | 1:2.5.2-1ubuntu0.1 | GPL-2+, LGPL-2+ | `ubuntu/attr_2.5.2-1ubuntu0.1.dsc` |
| libaudit-common | 1:3.1.2-2.1build1.1 | GPL-2, LGPL-2.1 | `ubuntu/audit_3.1.2-2.1build1.1.dsc` |
| libaudit1 | 1:3.1.2-2.1build1.1 | GPL-2, LGPL-2.1 | `ubuntu/audit_3.1.2-2.1build1.1.dsc` |
| libblkid1 | 2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| libbpf1 | 1:1.3.0-2build2 | LGPL-2.1 or BSD-2-Clause, GPL-2.0, GPL-2 with Linux-syscall-note exception, GPL-2 with Linux-syscall-note exception OR BSD-3-Clause, GPL-2+, LGPL-2.1, BSD-2-Clause, BSD-3-Clause | `ubuntu/libbpf_1.3.0-2build2.dsc` |
| libbrotli1 | 1.1.0-2build2 | MIT | `ubuntu/brotli_1.1.0-2build2.dsc` |
| libbsd0 | 0.12.1-1build1.1 | BSD-3-clause, BSD-3-clause-Regents, BSD-3-clause-Regents and BSD-2-clause-NetBSD, BSD-3-clause-author, BSD-3-clause-John-Birrell, BSD-5-clause-Peter-Wemm, BSD-2-clause-NetBSD, BSD-2-clause, BSD-2-clause-verbatim, BSD-2-clause-author, ISC, ISC-Original, Expat, public-domain, Beerware | `ubuntu/libbsd_0.12.1-1build1.1.dsc` |
| libbz2-1.0 | 1.0.8-5.1ubuntu0.1 | BSD-variant, GPL-2 | `ubuntu/bzip2_1.0.8-5.1ubuntu0.1.dsc` |
| libc-bin | 2.39-0ubuntu8.9 | see-copyright | `ubuntu/glibc_2.39-0ubuntu8.9.dsc` |
| libc6 | 2.39-0ubuntu8.9 | see-copyright | `ubuntu/glibc_2.39-0ubuntu8.9.dsc` |
| libc6-dev (guest tools) | 2.39-0ubuntu8.9 | see the source's debian/copyright | `ubuntu/glibc_2.39-0ubuntu8.9.dsc` |
| libcap-ng0 | 0.8.4-2build2 | LGPL-2.1+, GPL-2+, GPL-3 | `ubuntu/libcap-ng_0.8.4-2build2.dsc` |
| libcap2 | 1:2.66-5ubuntu2.4 | BSD-3-clause or GPL-2, BSD-3-clause or GPL-2+, BSD-3-clause, GPL-2, GPL-2+ | `ubuntu/libcap2_2.66-5ubuntu2.4.dsc` |
| libcap2-bin | 1:2.66-5ubuntu2.4 | BSD-3-clause or GPL-2, BSD-3-clause or GPL-2+, BSD-3-clause, GPL-2, GPL-2+ | `ubuntu/libcap2_2.66-5ubuntu2.4.dsc` |
| libcom-err2 | 1.47.0-2.4~exp1ubuntu4.1 | GPL-2, LGPL-2, BSD-3-Clause, Apache-2, ISC, GPL or MIT-US-export, Kazlib, Latex2e, GPL-2+ with Texinfo exception | `ubuntu/e2fsprogs_1.47.0-2.4~exp1ubuntu4.1.dsc` |
| libcrypt1 | 1:4.4.36-4build1 | see-copyright | `ubuntu/libxcrypt_4.4.36-4build1.dsc` |
| libcryptsetup12 | 2:2.7.0-1ubuntu4.2 | GPL-2+ with OpenSSL exception, GPL-2+, GPL-3+, LGPL-2.1+, LGPL-2.1+ with OpenSSL exception, CC0 or Apache-2.0, public-domain, CC0, Apache-2.0 | `ubuntu/cryptsetup_2.7.0-1ubuntu4.2.dsc` |
| libcurl4t64 | 8.5.0-2ubuntu10.15 | curl, OLDAP-2.8, ISC, GPL-2+ with Autoconf-data exception, GPL-3+ with Autoconf-data exception, GPL-2+ with Libtool exception, BSD-3-clause, BSD-4-Clause-UC, FSFULLR, X11, BSD-3-Clause | `ubuntu/curl_8.5.0-2ubuntu10.15.dsc` |
| libdb5.3t64 | 5.3.28+dfsg2-7 | Sleepycat and BSD-3-clause, Sleepycat, BSD-3-clause, Ms-PL, GPL or Artistic, X11, MIT-old, TCL-like, BSD-3-clause-fjord, GPL-3, zlib, Artistic or BSD-3-clause, GPL | `ubuntu/db5.3_5.3.28+dfsg2-7.dsc` |
| libdbus-1-3 | 1.14.10-4ubuntu4.1 | GPL-2+ or AFL-2.1, and Expat and Tcl-BSDish, GPL-2+, BSD-3-clause and BSD-3-clause-generic, autoconf-archive-permissive, FSF-unlimited-permission, LGPL-2.1+, g10-permissive, Expat, Tcl-BSDish, BSD-3-clause, BSD-3-clause-generic, AFL-2.1 | `ubuntu/dbus_1.14.10-4ubuntu4.1.dsc` |
| libdebconfclient0 | 0.271ubuntu3 | BSD-2-Clause, GPL-2+, BSD-2-clause | `ubuntu/cdebconf_0.271ubuntu3.dsc` |
| libdevmapper1.02.1 | 2:1.02.185-3ubuntu3.2 | GPL-2.0, LGPL-2.1, BSD-2-Clause, GPL-2.0+ | `ubuntu/lvm2_2.03.16-3ubuntu3.2.dsc` |
| libedit2 | 3.1-20230828-1build1 | BSD-3-clause | `ubuntu/libedit_3.1-20230828-1build1.dsc` |
| libelf1t64 | 0.190-1.1ubuntu0.1 | GPL-3+, LGPL-3+ or GPL-2+, BSD-2-clause, GFDL-NIV-1.3, LGPL-2.1+, GPL-3+ with Bison exception | `ubuntu/elfutils_0.190-1.1ubuntu0.1.dsc` |
| libexpat1 | 2.6.1-2ubuntu0.6 | MIT | `ubuntu/expat_2.6.1-2ubuntu0.6.dsc` |
| libext2fs2t64 | 1.47.0-2.4~exp1ubuntu4.1 | GPL-2, LGPL-2, BSD-3-Clause, Apache-2, ISC, GPL or MIT-US-export, Kazlib, Latex2e, GPL-2+ with Texinfo exception | `ubuntu/e2fsprogs_1.47.0-2.4~exp1ubuntu4.1.dsc` |
| libfdisk1 | 2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| libffi8 | 3.4.6-1build1 | Expat, X11, GPL-2+, GPL-3+, MPL-1.1 or GPL-2+ or LGPL-2.1+, public-domain, GPL | `ubuntu/libffi_3.4.6-1build1.dsc` |
| libgcc-s1 | 14.2.0-4ubuntu2~24.04.1 | see-copyright | `ubuntu/gcc-14_14.2.0-4ubuntu2~24.04.1.dsc` |
| libgcrypt20 | 1.10.3-2ubuntu0.2 | see-copyright | `ubuntu/libgcrypt20_1.10.3-2ubuntu0.2.dsc` |
| libgmp10 | 2:6.3.0+dfsg-2ubuntu6.1 | GPL-2+ or LGPL-3+, GPL-3+, GPL-3+ with Bison exception, GPL-2+, LGPL-3+ | `ubuntu/gmp_6.3.0+dfsg-2ubuntu6.1.dsc` |
| libgnutls30t64 | 3.8.3-1.1ubuntu3.6 | The main library is licensed under GNU Lesser, CC0 license, Expat, Apache-2.0, LGPLv3+_or_GPLv2+, LGPLv2.1+, GPLv3+, BSD-3-Clause | `ubuntu/gnutls28_3.8.3-1.1ubuntu3.6.dsc` |
| libgpg-error0 | 1.47-3build2.1 | LGPL-2.1+, LGPL-2.1+ or BSD-3-clause, g10-permissive, GPL-3+, BSD-3-clause | `ubuntu/libgpg-error_1.47-3build2.1.dsc` |
| libgssapi-krb5-2 | 1.20.1-6ubuntu2.10 | see-copyright | `ubuntu/krb5_1.20.1-6ubuntu2.10.dsc` |
| libhogweed6t64 | 3.9.1-2.2build1.1 | LGPL-3+ or GPL-2+, LGPL-2+, Expat, GPL-2+, GPL-3+ with Autoconf exception, public-domain, GPL-2, GAP, LGPL-3+ | `ubuntu/nettle_3.9.1-2.2build1.1.dsc` |
| libicu74 | 74.2-1ubuntu3.1 | MIT, GPL-3 | `ubuntu/icu_74.2-1ubuntu3.1.dsc` |
| libidn2-0 | 2.3.7-2build1.1 | GPL-3+, LGPL-3+ or GPL-2+, Unicode, GPL-2+, LGPL-3+ | `ubuntu/libidn2_2.3.7-2build1.1.dsc` |
| libip4tc2 | 1.8.10-3ubuntu2 | GPL-2, Artistic, GPL-2+, custom | `ubuntu/iptables_1.8.10-3ubuntu2.dsc` |
| libip6tc2 | 1.8.10-3ubuntu2 | GPL-2, Artistic, GPL-2+, custom | `ubuntu/iptables_1.8.10-3ubuntu2.dsc` |
| libjansson4 | 2.14-2build2 | Expat | `ubuntu/jansson_2.14-2build2.dsc` |
| libjq1 | 1.7.1-3ubuntu0.24.04.2 | MIT, CC-BY-3.0, Expat, GPL-2.0+ | `ubuntu/jq_1.7.1-3ubuntu0.24.04.2.dsc` |
| libjson-c5 | 0.17-1build1 | Expat | `ubuntu/json-c_0.17-1build1.dsc` |
| libk5crypto3 | 1.20.1-6ubuntu2.10 | see-copyright | `ubuntu/krb5_1.20.1-6ubuntu2.10.dsc` |
| libkeyutils1 | 1.6.3-3build1 | GPL-2+, LGPL-2+ | `ubuntu/keyutils_1.6.3-3build1.dsc` |
| libkmod2 | 31+20240202-2ubuntu7.2 | LGPL-2.1+, GPL-2+ | `ubuntu/kmod_31+20240202-2ubuntu7.2.dsc` |
| libkrb5-3 | 1.20.1-6ubuntu2.10 | see-copyright | `ubuntu/krb5_1.20.1-6ubuntu2.10.dsc` |
| libkrb5support0 | 1.20.1-6ubuntu2.10 | see-copyright | `ubuntu/krb5_1.20.1-6ubuntu2.10.dsc` |
| libksba8 | 1.6.6-1build1 | FSFUL, LGPL-2.1-or-later | `ubuntu/libksba_1.6.6-1build1.dsc` |
| libldap2 | 2.6.10+dfsg-0ubuntu0.24.04.1 | OpenLDAP-2.8, FSF-unlimited and GPL-2+ with Autoconf exception, GPL-3+ with Autoconf exception, FSF-unlimited, GPL-2+ with Libtool exception and GPL-3+ with Libtool exception and GPL-3+, GPL-2+ with Autoconf exception, GPL-2+, OpenLDAP-2.8 and UMich and F5, OpenLDAP-2.8 and UMich, OpenLDAP-2.8 and JCG and UMich, OpenLDAP-2.8 and FSF-unlimited and GPL-2+ with Libtool exception, FSF-unlimited and GPL-2+ with Libtool exception, FSF-unlimited and OpenLDAP-2.8, MIT-XC, NeoSoft-permissive, BSD-3-clause, OpenLDAP-2.8 and BSD-3-clause, OpenLDAP-2.8 and Beerware, public-domain, OpenLDAP-2.8 and BSD-4-clause-California, OpenLDAP-2.8 and BSD-3-clause-variant, OpenLDAP-2.8 and Expat-ISC, OpenLDAP-2.8 and Expat-UNM, OpenLDAP-2.8 and Expat, BSD-3-clause-California, F5, GPL-2+ with Libtool exception, GPL-3+, GPL-3+ with Libtool exception, JCG, UMich, Beerware, BSD-3-clause-variant, Expat-ISC, Expat-UNM, Expat, BSD-4-clause-California | `ubuntu/openldap_2.6.10+dfsg-0ubuntu0.24.04.1.dsc` |
| liblmdb0 | 0.9.31-1build1 | OpenLDAP-2.8 | `ubuntu/lmdb_0.9.31-1build1.dsc` |
| liblz4-1 | 1.9.4-1build1.1 | BSD-2-clause, GPL-2+, GPL-2+ or BSD-2-clause | `ubuntu/lz4_1.9.4-1build1.1.dsc` |
| liblzma5 | 5.6.1+really5.4.5-1ubuntu0.3 | PD, probably-PD, GPL-2+, LGPL-2.1+, permissive-fsf, Autoconf, permissive-nowarranty, GPL-2, none, config-h, noderivs, PD-debian | `ubuntu/xz-utils_5.6.1+really5.4.5-1ubuntu0.3.dsc` |
| libmaxminddb0 | 1.9.1-1build1 | Apache-2.0, CC-BY-SA-3.0, GPL-2+, BSD-2-clause and BSD-3-clause and BSD-4-clause | `ubuntu/libmaxminddb_1.9.1-1build1.dsc` |
| libmd0 | 1.1.0-2build1.1 | BSD-3-clause, BSD-3-clause-Aaron-D-Gifford, BSD-2-clause, BSD-2-clause-NetBSD, ISC, Beerware, public-domain-md4, public-domain-md5, public-domain-sha1 | `ubuntu/libmd_1.1.0-2build1.1.dsc` |
| libmnl0 | 1.0.5-2build1 | LGPL-2.1, GPL-2+ | `ubuntu/libmnl_1.0.5-2build1.dsc` |
| libmount1 | 2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| libmpfr6 | 4.2.1-1build1.1 | see-copyright | `ubuntu/mpfr4_4.2.1-1build1.1.dsc` |
| libncursesw6 | 6.4+20240113-1ubuntu2.2 | MIT/X11, X11, BSD-3-clause | `ubuntu/ncurses_6.4+20240113-1ubuntu2.2.dsc` |
| libnetfilter-conntrack3 | 1.0.9-6build1 | GPL-2+ | `ubuntu/libnetfilter-conntrack_1.0.9-6build1.dsc` |
| libnettle8t64 | 3.9.1-2.2build1.1 | LGPL-3+ or GPL-2+, LGPL-2+, Expat, GPL-2+, GPL-3+ with Autoconf exception, public-domain, GPL-2, GAP, LGPL-3+ | `ubuntu/nettle_3.9.1-2.2build1.1.dsc` |
| libnfnetlink0 | 1.0.2-2build1 | GPL-2.0+ | `ubuntu/libnfnetlink_1.0.2-2build1.dsc` |
| libnftables1 | 1.0.9-1ubuntu0.1 | GPL-2, GPL-2+, CC-BY-SA-4.0 | `ubuntu/nftables_1.0.9-1ubuntu0.1.dsc` |
| libnftnl11 | 1.2.6-2build1 | GPL-2+, GPL-2 | `ubuntu/libnftnl_1.2.6-2build1.dsc` |
| libnghttp2-14 | 1.59.0-1ubuntu0.4 | Expat, all-permissive, GPL-3+ with autoconf exception, MIT, BSD-2-clause | `ubuntu/nghttp2_1.59.0-1ubuntu0.4.dsc` |
| libnpth0t64 | 1.6-3.1build1 | LGPL-2.1+ | `ubuntu/npth_1.6-3.1build1.dsc` |
| libonig5 | 6.9.9-1build1 | BSD-2-clause, GPL-2+ | `ubuntu/libonig_6.9.9-1build1.dsc` |
| libp11-kit0 | 0.25.3-4ubuntu2.2 | BSD-3-clause, FSFULLR, GPL-2+ with Autoconf-data exception, GPL-3+ with Autoconf-data exception, X11, ISC, customFSFULLRWD, Apache-2.0, LGPL-2.1+, customFSFUL, FSFAP | `ubuntu/p11-kit_0.25.3-4ubuntu2.2.dsc` |
| libpam-modules | 1.5.3-5ubuntu5.7 | BSD-3-clause or GPL, GPL-2, GPL-2+, GPL-3, GPL-3+ with Bison exception, BSD-tcp_wrappers, LGPL-2+, public-domain, Beerware | `ubuntu/pam_1.5.3-5ubuntu5.7.dsc` |
| libpam-modules-bin | 1.5.3-5ubuntu5.7 | BSD-3-clause or GPL, GPL-2, GPL-2+, GPL-3, GPL-3+ with Bison exception, BSD-tcp_wrappers, LGPL-2+, public-domain, Beerware | `ubuntu/pam_1.5.3-5ubuntu5.7.dsc` |
| libpam-runtime | 1.5.3-5ubuntu5.7 | BSD-3-clause or GPL, GPL-2, GPL-2+, GPL-3, GPL-3+ with Bison exception, BSD-tcp_wrappers, LGPL-2+, public-domain, Beerware | `ubuntu/pam_1.5.3-5ubuntu5.7.dsc` |
| libpam-systemd | 255.4-1ubuntu8.17 | LGPL-2.1+, CC0-1.0, GPL-2 with Linux-syscall-note exception, Expat, public-domain, GPL-2+ | `ubuntu/systemd_255.4-1ubuntu8.17.dsc` |
| libpam0g | 1.5.3-5ubuntu5.7 | BSD-3-clause or GPL, GPL-2, GPL-2+, GPL-3, GPL-3+ with Bison exception, BSD-tcp_wrappers, LGPL-2+, public-domain, Beerware | `ubuntu/pam_1.5.3-5ubuntu5.7.dsc` |
| libpcre2-8-0 | 10.42-4ubuntu2.1 | BSD-3-clause-Cambridge with BINARY LIBRARY-LIKE PACKAGES exception, BSD-3-clause, X11, BSD-2-clause, public-domain | `ubuntu/pcre2_10.42-4ubuntu2.1.dsc` |
| libproc2-0 | 2:4.0.4-4ubuntu3.3 | LGPL-2.1+, LGPL-2.0+, GPL-2.0+ | `ubuntu/procps_4.0.4-4ubuntu3.3.dsc` |
| libpsl5t64 | 0.21.2-1.1build1 | MIT, gnulib, Chromium | `ubuntu/libpsl_0.21.2-1.1build1.dsc` |
| libreadline8t64 | 8.2-4build1 | GPL-3+, GPL-2+, GFDL-NIV-1.3+, ISC-no-attribution, GPL-3 | `ubuntu/readline_8.2-4build1.dsc` |
| librtmp1 | 2.4+20151223.gitfa8646d.1-2build7 | see-copyright | `ubuntu/rtmpdump_2.4+20151223.gitfa8646d.1-2build7.dsc` |
| libsasl2-2 | 2.1.28+dfsg1-5ubuntu3.1 | BSD-3-Clause-Attribution, BSD-3-clause, BSD-2-clause, GPL-3+, GPL-3, BSD-4-clause-UC, RSA-MD, BSD-3-Clause-Attribution and IBM-as-is, BSD-3-clause-JANET and BSD-3-Clause-Attribution, BSD-3-clause-PADL and MIT-OpenVision, MIT-OpenVision, OpenLDAP, FSFULLR and MIT-CMU, BSD-3-Clause-Attribution and MIT-Export, BSD-2-clause and MIT-CMU, BSD-2.2-clause, FSFULLR, MIT-Export, MIT-CMU, BSD-3-clause-JANET, BSD-3-clause-PADL, IBM-as-is | `ubuntu/cyrus-sasl2_2.1.28+dfsg1-5ubuntu3.1.dsc` |
| libsasl2-modules-db | 2.1.28+dfsg1-5ubuntu3.1 | BSD-3-Clause-Attribution, BSD-3-clause, BSD-2-clause, GPL-3+, GPL-3, BSD-4-clause-UC, RSA-MD, BSD-3-Clause-Attribution and IBM-as-is, BSD-3-clause-JANET and BSD-3-Clause-Attribution, BSD-3-clause-PADL and MIT-OpenVision, MIT-OpenVision, OpenLDAP, FSFULLR and MIT-CMU, BSD-3-Clause-Attribution and MIT-Export, BSD-2-clause and MIT-CMU, BSD-2.2-clause, FSFULLR, MIT-Export, MIT-CMU, BSD-3-clause-JANET, BSD-3-clause-PADL, IBM-as-is | `ubuntu/cyrus-sasl2_2.1.28+dfsg1-5ubuntu3.1.dsc` |
| libseccomp2 | 2.5.5-1ubuntu3.1 | LGPL-2.1 | `ubuntu/libseccomp_2.5.5-1ubuntu3.1.dsc` |
| libselinux1 | 3.5-2ubuntu2.1 | public-domain, GPL-2 | `ubuntu/libselinux_3.5-2ubuntu2.1.dsc` |
| libsemanage-common | 3.5-1build5 | LGPL-2.1+, GPL-2 | `ubuntu/libsemanage_3.5-1build5.dsc` |
| libsemanage2 | 3.5-1build5 | LGPL-2.1+, GPL-2 | `ubuntu/libsemanage_3.5-1build5.dsc` |
| libsepol2 | 3.5-2build1 | LGPL-2.1+, Zlib, GPL-2, GPL-2+ | `ubuntu/libsepol_3.5-2build1.dsc` |
| libsigsegv2 | 2.14-1ubuntu2 | GPL-2+, GPL-3+ with Autoconf exception, Expat, GPL-2+ with Autoconf exception, FSFULL, permissive | `ubuntu/libsigsegv_2.14-1ubuntu2.dsc` |
| libsmartcols1 | 2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| libsqlite3-0 | 3.45.1-1ubuntu2.8 | public-domain, GPL-2+ | `ubuntu/sqlite3_3.45.1-1ubuntu2.8.dsc` |
| libss2 | 1.47.0-2.4~exp1ubuntu4.1 | GPL-2, LGPL-2, BSD-3-Clause, Apache-2, ISC, GPL or MIT-US-export, Kazlib, Latex2e, GPL-2+ with Texinfo exception | `ubuntu/e2fsprogs_1.47.0-2.4~exp1ubuntu4.1.dsc` |
| libssh-4 | 0.10.6-2ubuntu0.5 | LGPL-2.1+~OpenSSL, public-domain, LGPL-2.1, BSD-2-clause, BSD-3-clause, LGPL-2.1+~OpenSSL or BSD-2-clause or BSD-3-clause | `ubuntu/libssh_0.10.6-2ubuntu0.5.dsc` |
| libssl3t64 | 3.0.13-0ubuntu3.16 | Apache-2.0, Artistic or GPL-1+, Artistic, GPL-1+ | `ubuntu/openssl_3.0.13-0ubuntu3.16.dsc` |
| libstdc++6 | 14.2.0-4ubuntu2~24.04.1 | see-copyright | `ubuntu/gcc-14_14.2.0-4ubuntu2~24.04.1.dsc` |
| libsystemd-shared | 255.4-1ubuntu8.17 | LGPL-2.1+, CC0-1.0, GPL-2 with Linux-syscall-note exception, Expat, public-domain, GPL-2+ | `ubuntu/systemd_255.4-1ubuntu8.17.dsc` |
| libsystemd0 | 255.4-1ubuntu8.17 | LGPL-2.1+, CC0-1.0, GPL-2 with Linux-syscall-note exception, Expat, public-domain, GPL-2+ | `ubuntu/systemd_255.4-1ubuntu8.17.dsc` |
| libtasn1-6 | 4.19.0-3ubuntu0.24.04.2 | see-copyright | `ubuntu/libtasn1-6_4.19.0-3ubuntu0.24.04.2.dsc` |
| libtinfo6 | 6.4+20240113-1ubuntu2.2 | MIT/X11, X11, BSD-3-clause | `ubuntu/ncurses_6.4+20240113-1ubuntu2.2.dsc` |
| libudev1 | 255.4-1ubuntu8.17 | LGPL-2.1+, CC0-1.0, GPL-2 with Linux-syscall-note exception, Expat, public-domain, GPL-2+ | `ubuntu/systemd_255.4-1ubuntu8.17.dsc` |
| libunistring5 | 1.1-2build1.1 | LGPL-3+ or GPL-2+, FreeSoftware, GPL-3+ or GFDL-NIV-1.2+, GPL-3+, GPL-2+, GPL-2+ with distribution exception, GPL-2+ with distribution exception, Expat and GPL-2+, X11, LGPL-3+, GFDL-NIV-1.2+ | `ubuntu/libunistring_1.1-2build1.1.dsc` |
| libuuid1 | 2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| libuv1t64 | 1.48.0-1.1build1 | Expat, ISC, CC-BY-4.0, Apache-2.0, BSD-2-clause, GPL3+ with autoconf exception | `ubuntu/libuv1_1.48.0-1.1build1.dsc` |
| libxml2 | 2.9.14+dfsg-1.3ubuntu3.9 | MIT-1, ISC | `ubuntu/libxml2_2.9.14+dfsg-1.3ubuntu3.9.dsc` |
| libxtables12 | 1.8.10-3ubuntu2 | GPL-2, Artistic, GPL-2+, custom | `ubuntu/iptables_1.8.10-3ubuntu2.dsc` |
| libxxhash0 | 0.8.2-2build1 | BSD-2-clause, GPL-2+, BSD-2-clause or GPL-2+ | `ubuntu/xxhash_0.8.2-2build1.dsc` |
| libzstd1 | 1.5.5+dfsg2-2build1.1 | BSD-3-clause or GPL-2, zlib, Expat, GPL-2, BSD-3-clause | `ubuntu/libzstd_1.5.5+dfsg2-2build1.1.dsc` |
| login | 1:4.13+dfsg1-4ubuntu3.2 | BSD-3-clause, GPL-1, GPL-2+, public-domain | `ubuntu/shadow_4.13+dfsg1-4ubuntu3.2.dsc` |
| logsave | 1.47.0-2.4~exp1ubuntu4.1 | GPL-2, LGPL-2, BSD-3-Clause, Apache-2, ISC, GPL or MIT-US-export, Kazlib, Latex2e, GPL-2+ with Texinfo exception | `ubuntu/e2fsprogs_1.47.0-2.4~exp1ubuntu4.1.dsc` |
| mawk | 1.3.4.20240123-1build1 | GPL-2.0-only, X11, CC-BY-3.0 | `ubuntu/mawk_1.3.4.20240123-1build1.dsc` |
| mount | 2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| ncurses-base | 6.4+20240113-1ubuntu2.2 | MIT/X11, X11, BSD-3-clause | `ubuntu/ncurses_6.4+20240113-1ubuntu2.2.dsc` |
| ncurses-bin | 6.4+20240113-1ubuntu2.2 | MIT/X11, X11, BSD-3-clause | `ubuntu/ncurses_6.4+20240113-1ubuntu2.2.dsc` |
| netbase | 6.4 | GPL-2 | `ubuntu/netbase_6.4.dsc` |
| netcat-openbsd | 1.226-1ubuntu2 | BSD-3-Clause, BSD-2-Clause | `ubuntu/netcat-openbsd_1.226-1ubuntu2.dsc` |
| nftables | 1.0.9-1ubuntu0.1 | GPL-2, GPL-2+, CC-BY-SA-4.0 | `ubuntu/nftables_1.0.9-1ubuntu0.1.dsc` |
| openssl | 3.0.13-0ubuntu3.16 | Apache-2.0, Artistic or GPL-1+, Artistic, GPL-1+ | `ubuntu/openssl_3.0.13-0ubuntu3.16.dsc` |
| passwd | 1:4.13+dfsg1-4ubuntu3.2 | BSD-3-clause, GPL-1, GPL-2+, public-domain | `ubuntu/shadow_4.13+dfsg1-4ubuntu3.2.dsc` |
| perl-base | 5.38.2-3.2ubuntu0.6 | GPL-1+ or Artistic, Expat, REGCOMP, and GPL-1+ or Artistic, GPL-3+-WITH-BISON-EXCEPTION, Unicode, GPL-1+ or Artistic, and Unicode, BZIP, ZLIB, Artistic, GPL-2+ or Artistic, Expat or GPL-1+ or Artistic, FSFAP, BSD-3-clause-with-weird-numbering, CC0-1.0, TEXT-TABS, GPL-1+ or Artistic, and BSD-4-clause-POWERDOG, GPL-1+ or Artistic, and BSD-3-clause-GENERIC, BSD-3-clause, SDBM-PUBLIC-DOMAIN, DONT-CHANGE-THE-GPL, GPL-1+ or Artistic or Artistic-dist, Artistic-dist, Artistic or GPL-1+ or Artistic-dist, GPL-1+ or Artistic, and Expat, LGPL-2.1, GPL-1+, GPL-2+, Artistic-2, BSD-4-clause-POWERDOG, BSD-3-clause-GENERIC, REGCOMP | `ubuntu/perl_5.38.2-3.2ubuntu0.6.dsc` |
| pinentry-curses | 1.2.1-3ubuntu5 | GPL-2+, GPL-2, X11, LGPL-3+ or GPL-2+, LGPL-3+ | `ubuntu/pinentry_1.2.1-3ubuntu5.dsc` |
| procps | 2:4.0.4-4ubuntu3.3 | LGPL-2.1+, LGPL-2.0+, GPL-2.0+ | `ubuntu/procps_4.0.4-4ubuntu3.3.dsc` |
| readline-common | 8.2-4build1 | GPL-3+, GPL-2+, GFDL-NIV-1.3+, ISC-no-attribution, GPL-3 | `ubuntu/readline_8.2-4build1.dsc` |
| sed | 4.9-2ubuntu0.24.04.1 | GPL-3+, X11, GFDL-NIV-1.3+, GPL-3+ and ISC, ISC, BSD-4-clause-UC, BSL-1, pcre | `ubuntu/sed_4.9-2ubuntu0.24.04.1.dsc` |
| sensible-utils | 0.0.22 | GPL-2+, All-permissive, configure, installsh, BSD-2-clause | `ubuntu/sensible-utils_0.0.22.dsc` |
| systemd | 255.4-1ubuntu8.17 | LGPL-2.1+, CC0-1.0, GPL-2 with Linux-syscall-note exception, Expat, public-domain, GPL-2+ | `ubuntu/systemd_255.4-1ubuntu8.17.dsc` |
| systemd-dev | 255.4-1ubuntu8.17 | LGPL-2.1+, CC0-1.0, GPL-2 with Linux-syscall-note exception, Expat, public-domain, GPL-2+ | `ubuntu/systemd_255.4-1ubuntu8.17.dsc` |
| systemd-sysv | 255.4-1ubuntu8.17 | LGPL-2.1+, CC0-1.0, GPL-2 with Linux-syscall-note exception, Expat, public-domain, GPL-2+ | `ubuntu/systemd_255.4-1ubuntu8.17.dsc` |
| sysvinit-utils | 3.08-6ubuntu3 | GPL-2.0+, LGPL-2.1+, GPL-3.0, GPL-2.0 | `ubuntu/sysvinit_3.08-6ubuntu3.dsc` |
| tar | 1.35+dfsg-3ubuntu0.4 | GPL-3+, GPL-3+ with Bison exception, LGPL-2.1+, LGPL-3+, GPL-2+ | `ubuntu/tar_1.35+dfsg-3ubuntu0.4.dsc` |
| ubuntu-keyring | 2023.11.28.1 | see-copyright | `ubuntu/ubuntu-keyring_2023.11.28.1.dsc` |
| unminimize | 0.2.1 | GPL-2.0+ | `ubuntu/unminimize_0.2.1.dsc` |
| util-linux | 2.39.3-9ubuntu6.6 | GPL-2+, GPL-2, GPL-3+, public-domain, BSD-4-clause, MIT, BSD-3-clause, BSLA, LGPL-2+, LGPL-2.1+, LGPL, LGPL-3+ | `ubuntu/util-linux_2.39.3-9ubuntu6.6.dsc` |
| xz-utils | 5.6.1+really5.4.5-1ubuntu0.3 | PD, probably-PD, GPL-2+, LGPL-2.1+, permissive-fsf, Autoconf, permissive-nowarranty, GPL-2, none, config-h, noderivs, PD-debian | `ubuntu/xz-utils_5.6.1+really5.4.5-1ubuntu0.3.dsc` |
| zlib1g | 1:1.3.dfsg-3.1ubuntu2.2 | Zlib | `ubuntu/zlib_1.3.dfsg-3.1ubuntu2.2.dsc` |

## Alpine packages (Alpine 3.20.6, the control filesystem)

| Package | Version | Licence | Source |
| --- | --- | --- | --- |
| alpine-baselayout | 3.6.5-r0 | GPL-2.0-only | `alpine/alpine-baselayout/` (APKBUILD and upstream files) |
| alpine-baselayout-data | 3.6.5-r0 | GPL-2.0-only | `alpine/alpine-baselayout/` (APKBUILD and upstream files) |
| alpine-keys | 2.4-r1 | MIT | `alpine/alpine-keys/` (APKBUILD and upstream files) |
| apk-tools | 2.14.4-r1 | GPL-2.0-only | `alpine/apk-tools/` (APKBUILD and upstream files) |
| busybox | 1.36.1-r29 | GPL-2.0-only | `alpine/busybox/` (APKBUILD and upstream files) |
| busybox-binsh | 1.36.1-r29 | GPL-2.0-only | `alpine/busybox/` (APKBUILD and upstream files) |
| ca-certificates-bundle | 20241121-r1 | MPL-2.0 AND MIT | `alpine/ca-certificates/` (APKBUILD and upstream files) |
| libcrypto3 | 3.3.3-r0 | Apache-2.0 | `alpine/openssl/` (APKBUILD and upstream files) |
| libssl3 | 3.3.3-r0 | Apache-2.0 | `alpine/openssl/` (APKBUILD and upstream files) |
| musl | 1.2.5-r1 | MIT | `alpine/musl/` (APKBUILD and upstream files) |
| musl-utils | 1.2.5-r1 | MIT AND BSD-2-Clause AND GPL-2.0-or-later | `alpine/musl/` (APKBUILD and upstream files) |
| scanelf | 1.3.7-r2 | GPL-2.0-only | `alpine/pax-utils/` (APKBUILD and upstream files) |
| ssl_client | 1.36.1-r29 | GPL-2.0-only | `alpine/busybox/` (APKBUILD and upstream files) |
| zlib | 1.3.1-r1 | Zlib | `alpine/zlib/` (APKBUILD and upstream files) |

## Koala's guest helpers

Apache-2.0, statically linked with glibc (LGPL-2.1; its source is `ubuntu/glibc_*.dsc`).
Their sources are in `helpers/`.
