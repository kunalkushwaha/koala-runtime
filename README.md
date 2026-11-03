# koala-runtime

The source code for the open-source parts of the
[Koala](https://github.com/kunalkushwaha/homebrew-koala) VM runtimes.

Koala runs Linux VMs on macOS and Linux. Each VM boots a **runtime**: a
Linux kernel and disk images, published as signed bundles at
`ghcr.io/kunalkushwaha/koala-runtime`. The runtimes contain software under the
GNU GPL and LGPL (the Linux kernel, busybox, Ubuntu's and Alpine's
packages). This repository gives you the matching source for each runtime
that is published.

## What is here

| Runtime | Tag | Source |
| --- | --- | --- |
| Linux x86_64 | `0.1.0-linux-amd64.5` | [`0.1.0-linux-amd64.5/`](0.1.0-linux-amd64.5/SOURCES.md) and the release [`0.1.0-linux-amd64.5`](../../releases/tag/0.1.0-linux-amd64.5) |
| macOS (Apple silicon) | `0.1.0-macos-arm64.1` | [`0.1.0-macos-arm64.1/`](0.1.0-macos-arm64.1/SOURCES.md) and the release [`0.1.0-macos-arm64.1`](../../releases/tag/0.1.0-macos-arm64.1) |

For each runtime:
- **`SOURCES.md`** lists every component, its licence and version, and the
  file that holds its source.
- **`manifest.json`** is the same list for scripts, with each file's
  SHA-256.
- **The release** of the same name holds the source files themselves. The
  largest files are split into parts; rejoin them with `cat` (see the
  release's notes).

Each runtime's source set holds:
- **The Linux kernels:** the kernel.org tarball (Linux 6.18.60), each
  kernel's exact build configuration, and the scripts and configuration
  fragments that build them. Rebuilt with these, each kernel is
  byte-identical to the one in the runtime.
- **Ubuntu packages:** the Ubuntu source package of every package in the
  full VM's disk image, at the installed version.
- **Alpine packages:** for the small control filesystem, each package's
  build recipe from Alpine's `aports` and the upstream files it uses.
- **Koala's guest helpers** (Apache-2.0), which are statically linked with
  glibc (LGPL-2.1).

Docker's packages in the full image are Apache-2.0 and come from
[Docker's own repository](https://download.docker.com/linux/ubuntu); they
are listed in `SOURCES.md` with their upstream projects.

## Getting the source

Each runtime's source code is in this repository's release named after the
runtime: download the files listed there and check them with
`sha256sum -c SHA256SUMS`. If a file is missing or a link stops working,
open an issue in this repository.

## Credits

The runtimes are built on others' work:
- **[Kata Containers](https://github.com/kata-containers/kata-containers)**:
  the lean kernel's configuration starts from Kata 3.17.0's kernel config
  fragments (Apache-2.0, kept with their licence in each recipe), and its
  approach to small, fast VM kernels guided ours.
- **[Apple's containerization](https://github.com/apple/containerization)**:
  `vminitd`, `vmexec`, and the full kernel's base configuration and build
  script (Apache-2.0).
- **[The Linux kernel](https://kernel.org)**, **[Ubuntu](https://ubuntu.com)**,
  **[Alpine Linux](https://alpinelinux.org)** and
  **[busybox](https://busybox.net)**, whose software the runtimes contain.

## Licence

Each component keeps its own licence, given in `SOURCES.md` and in the
source files themselves. The files written for Koala (the build scripts and
fragments, the guest helpers) are Apache-2.0.
