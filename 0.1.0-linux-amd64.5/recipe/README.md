# The recipe of `0.1.0-linux-amd64.5`

The files Koala's runtime build used, as they were at the build's commit.

- **`L02/`** builds the two Linux kernels. `build-kernel.sh` builds the full
  kernel from containerization 0.47.0's `config-x86_64` and build script
  (both here, checked by digest) plus `kernel-x86_64.fragment`.
  `build-lean-kernel.sh` builds the lean kernel from `allnoconfig`, Kata
  Containers 3.17.0's fragments (`kernel/kata-3.17.0/`, Apache-2.0) and
  `kernel-lean-x86_64.fragment`. `config-full-x86_64` and
  `config-lean-x86_64` are the configurations each build produced; with
  them, `make vmlinux` on Linux 6.18.60 gives the shipped kernels. Both
  scripts run in Docker (Ubuntu 24.04 by digest), with
  `KBUILD_BUILD_TIMESTAMP="2026-01-01 00:00:00 UTC"`,
  `KBUILD_BUILD_USER=koala` and `KBUILD_BUILD_HOST=koala`.
- **`guest/`** builds the full VM's root image (`L02/build-full-root.sh` runs it for amd64): its `Dockerfile` (which also
  compiles Koala's guest helpers, statically linked with glibc) and every
  file it copies. The Ubuntu packages it installs are those in
  `runtime-packages-amd64.lock`; their sources are in this runtime's
  release.
