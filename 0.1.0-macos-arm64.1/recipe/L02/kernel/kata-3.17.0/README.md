# Kata Containers 3.17.0 kernel config fragments

Copied unchanged from
[kata-containers/kata-containers](https://github.com/kata-containers/kata-containers)
tag `3.17.0` (commit `a897bce29f3c79917a1508c958ebf0f57ea8dc2d`),
`tools/packaging/kernel/configs/fragments/common/*.conf` and `x86_64/*.conf`,
on 2026-11-03, and `arm64/*.conf` from the same commit later that day (MAC08,
#322), under the Apache License 2.0 (`LICENSE`, from the same commit).

Koala's lean kernel recipe ([R05B1](../../../../docs/tasks/R05B1.md),
`../../build-lean-kernel.sh`) merges a selection of them, as Kata's own
`build-kernel.sh` does, onto `allnoconfig`, then Koala's
`../../kernel-lean-x86_64.fragment` or `../../kernel-lean-arm64.fragment`. From here on they are part of Koala's
recipe: a change to one is a reviewed diff, and `build-lean-kernel.sh`
checks each file against the digests it pins.
