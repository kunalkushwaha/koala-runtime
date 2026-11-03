# The recipe of `0.1.0-macos-arm64.1`

The files Koala's Mac runtime build (`build-runtime.sh`) used, as they were
at the build's commit.

- **`L02/`** builds the two arm64 Linux kernels. `prepare-f08-kernel.sh`
  builds the full kernel from containerization 0.47.0's `config-arm64`,
  build script and builder image files (all here, checked by digest).
  `build-lean-kernel.sh arm64` builds the lean kernel from `allnoconfig`,
  Kata Containers 3.17.0's fragments (`kernel/kata-3.17.0/`, Apache-2.0) and
  `kernel-lean-arm64.fragment`. `config-full-arm64` and `config-lean-arm64`
  are the configurations each build produced; rebuilt with them, the
  kernels are byte-identical to the shipped ones. `prepare-f02.sh` and
  `prepare-f08.sh` make the lean and full disk images.
- **`guest/`** builds the full VM's root image: its `Dockerfile` (which also
  compiles Koala's guest helpers, statically linked with glibc) and every
  file it copies. The Ubuntu packages it installs are those in
  `runtime-packages.lock`; their sources are in this runtime's release.
