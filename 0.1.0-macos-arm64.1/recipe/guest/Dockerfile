ARG UBUNTU_IMAGE=docker.io/library/ubuntu@sha256:a853f94d226358a79c740cfc7bce0c289748f3fe3488d921d038ccd752c61b60
FROM ${UBUNTU_IMAGE} AS subreaper-builder

# Packages come only from runtime-packages.lock, verified by SHA-256 (R01A).
COPY koala-apt runtime-packages.lock packages-builder.txt packages-root.txt packages-docker.txt docker.asc /tmp/koala-apt/
COPY debs/ /tmp/koala-apt/debs/
RUN /tmp/koala-apt/koala-apt install builder && rm -rf /tmp/koala-apt && cp /bin/busybox /busybox
COPY koala-subreaper.c /tmp/koala-subreaper.c
RUN cc -O2 -Wall -Wextra -Werror -o /koala-subreaper /tmp/koala-subreaper.c
COPY koala-host-exec.c /tmp/koala-host-exec.c
RUN cc -O2 -Wall -Wextra -Werror -o /koala-host-exec /tmp/koala-host-exec.c
COPY koala-exec-status.c /tmp/koala-exec-status.c
RUN cc -static -O2 -Wall -Wextra -Werror -o /koala-exec-status /tmp/koala-exec-status.c
COPY koala-transfer.c /tmp/koala-transfer.c
RUN cc -static -O2 -Wall -Wextra -Werror -o /koala-transfer /tmp/koala-transfer.c
# R05B2: the guest's first program (sets the boot disks' readahead, then runs
# the real init); freestanding (no C library: glibc's static start-up asserts
# on the lean init file system's relative proc/self/exe link and kills init).
COPY koala-init.c /tmp/koala-init.c
RUN cc -static -nostdlib -ffreestanding -fno-stack-protector -O2 -Wall -Wextra -Werror \
      -o /koala-init /tmp/koala-init.c
RUN set -eux; \
    /koala-host-exec server & server=$!; \
    for attempt in 1 2 3 4 5; do test -S /run/koala-host-exec.sock && break; sleep 0.1; done; \
    set +e; \
    /koala-host-exec client 'printf broker-out; printf broker-err >&2; exit 7' \
      >/tmp/broker.stdout 2>/tmp/broker.stderr; \
    status=$?; \
    set -e; \
    test "$status" -eq 7; \
    test "$(cat /tmp/broker.stdout)" = broker-out; \
    test "$(cat /tmp/broker.stderr)" = broker-err; \
    /koala-host-exec client \
      'dd if=/dev/zero of=/tmp/broker-large bs=1M count=3 status=none; stat -c %s /tmp/broker-large' \
      | grep -qx 3145728; \
    install -m 0755 /koala-exec-status /usr/local/sbin/koala-exec-status; \
    /koala-host-exec client-v2 11111111-1111-4111-8111-111111111111 \
      0 0 /tmp 1 3 MARK=structured /bin/bash -c \
      'test "$MARK" = structured; test "$PWD" = /tmp; exit 137'; \
    grep -qx 'v1 exit 137' \
      /var/lib/koala/executions/11111111-1111-4111-8111-111111111111.result; \
    /koala-host-exec client-v2 22222222-2222-4222-8222-222222222222 \
      0 0 / 0 3 /bin/bash -c 'kill -9 $$'; \
    grep -qx 'v1 signal 9' \
      /var/lib/koala/executions/22222222-2222-4222-8222-222222222222.result; \
    /koala-host-exec client-v2 33333333-3333-4333-8333-333333333333 \
      0 0 / 0 1 /definitely-missing; \
    grep -qx 'v1 start_failed 2' \
      /var/lib/koala/executions/33333333-3333-4333-8333-333333333333.result; \
    printf 'stream-input\n' | \
      /koala-host-exec client-v3 0 44444444-4444-4444-8444-444444444444 \
        0 0 / 0 3 /bin/bash -c 'read value; printf "out:%s" "$value"; printf err >&2' | \
      grep -qx 'out:stream-input'; \
    grep -qx 'v1 exit 0' \
      /var/lib/koala/executions/44444444-4444-4444-8444-444444444444.result; \
    /koala-host-exec client-v3 0 55555555-5555-4555-8555-555555555555 \
      0 0 / 0 2 /bin/sleep 30 & streamed=$!; \
    sleep 0.1; kill -TERM "$streamed"; wait "$streamed"; \
    grep -qx 'v1 signal 15' \
      /var/lib/koala/executions/55555555-5555-4555-8555-555555555555.result; \
    kill "$server"; \
    wait "$server" 2>/dev/null || true

FROM ${UBUNTU_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# The package lists (with systemd and Docker version pins) are
# packages-root.txt and packages-docker.txt; the exact files are locked.
COPY koala-apt runtime-packages.lock packages-builder.txt packages-root.txt packages-docker.txt docker.asc /tmp/koala-apt/
COPY debs/ /tmp/koala-apt/debs/
RUN set -eux; \
    /tmp/koala-apt/koala-apt install root; \
    rm -rf /tmp/koala-apt; \
    update-alternatives --set iptables /usr/sbin/iptables-nft; \
    update-alternatives --set ip6tables /usr/sbin/ip6tables-nft; \
    rm -f /usr/sbin/policy-rc.d; \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*; \
    : >/etc/machine-id; \
    # Each VM's D-Bus ID is its own machine ID, not one baked into the image (R01A).
    ln -sf /etc/machine-id /var/lib/dbus/machine-id; \
    # Build-time logs and caches would differ between otherwise identical builds.
    rm -f /var/log/apt/history.log /var/log/apt/term.log /var/log/dpkg.log \
      /var/log/alternatives.log /var/cache/ldconfig/aux-cache; \
    mkdir -p /var/lib/koala-engine/docker /var/lib/koala-engine/containerd \
      /var/lib/docker /var/lib/containerd /var/lib/koala-sample /mnt/koala-host/root; \
    dpkg-query -W -f='${Package}\t${Version}\t${Architecture}\n' | LC_ALL=C sort \
      >/usr/share/koala-package-lock.txt

COPY vminitd /sbin/vminitd
COPY vmexec /sbin/vmexec
COPY --from=subreaper-builder /koala-subreaper /usr/local/sbin/koala-subreaper
COPY --from=subreaper-builder /koala-host-exec /usr/local/sbin/koala-host-exec
COPY --from=subreaper-builder /koala-exec-status /usr/local/sbin/koala-exec-status
COPY --from=subreaper-builder /koala-transfer /usr/local/sbin/koala-transfer
COPY --from=subreaper-builder /koala-init /sbin/koala-init
COPY koala-agent-wrapper /usr/local/sbin/koala-agent-wrapper
COPY koala-host-exec.service /etc/systemd/system/koala-host-exec.service
COPY koala-guest-agent.service /etc/systemd/system/koala-guest-agent.service
COPY var-lib-docker.mount /etc/systemd/system/var-lib-docker.mount
COPY var-lib-containerd.mount /etc/systemd/system/var-lib-containerd.mount
COPY koala-sample.service /etc/systemd/system/koala-sample.service
COPY koala-network.service /etc/systemd/system/koala-network.service
COPY resolv.conf /etc/koala/resolv.conf
COPY daemon.json /etc/docker/daemon.json
COPY containerd.toml /etc/containerd/config.toml

# R05B3: the second mask list is L18's E5a, boot work a VM doesn't need
# (~200 ms of a start); journald stays (readiness reads the boot journal, kept
# in /run). The sed makes the root the same whichever day it is built: the
# accounts the packages made record the build day as their last password
# change; one fixed day instead (2026-01-01, day 20454).
RUN set -eux; \
    chmod 0755 /sbin/vminitd /sbin/vmexec /sbin/koala-init /usr/local/sbin/koala-subreaper \
      /usr/local/sbin/koala-host-exec /usr/local/sbin/koala-exec-status \
      /usr/local/sbin/koala-transfer /usr/local/sbin/koala-agent-wrapper; \
    systemctl enable koala-host-exec.service koala-guest-agent.service koala-network.service \
      var-lib-docker.mount var-lib-containerd.mount \
      containerd.service docker.service; \
    systemctl set-default multi-user.target; \
    systemctl mask systemd-networkd.service systemd-networkd.socket \
      systemd-networkd-wait-online.service apt-daily.service apt-daily.timer \
      apt-daily-upgrade.service apt-daily-upgrade.timer; \
    systemctl mask systemd-random-seed.service systemd-update-utmp.service \
      systemd-timesyncd.service systemd-journal-flush.service e2scrub_reap.service \
      modprobe@dm_mod.service modprobe@loop.service modprobe@drm.service \
      modprobe@efi_pstore.service modprobe@configfs.service \
      sys-kernel-debug.mount dev-hugepages.mount; \
    for f in /etc/shadow /etc/shadow-; do [ ! -f "$f" ] || sed -i -E 's/^([^:]*:[^:]*:)[0-9]+:/\120454:/' "$f"; done; \
    test -s /etc/koala/resolv.conf

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
