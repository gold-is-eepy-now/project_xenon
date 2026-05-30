#!/bin/sh
# Copyright 2026 The Project Xenon Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Vendors the FreeBSD www/chromium port's patch set into this tree.
#
# Xenon aims to support FreeBSD in-tree rather than relying on the external
# FreeBSD ports `www/chromium` port. This script pulls that port's `patch-*`
# files (reproducibly, from a pinned commit) into build/freebsd/patches/ so the
# snapshot is vendored and version-controlled here.
#
# Usage:
#   sh build/freebsd/fetch_port_patches.sh            # use pinned commit
#   PORT_COMMIT=<sha> sh build/freebsd/fetch_port_patches.sh   # override
#
# After running, review the diff and update build/freebsd/MANIFEST (the commit,
# port version, and patch count are printed at the end).

set -eu

# --- Pinned upstream reference (keep in sync with build/freebsd/MANIFEST) ---
PORTS_REPO="https://github.com/freebsd/freebsd-ports.git"
PORT_COMMIT="${PORT_COMMIT:-8bb66f1b8cc8ef9a27b717284ddd6d7db5632a15}"
PORT_PATH="www/chromium/files"

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DEST="${SCRIPT_DIR}/patches"

WORK=$(mktemp -d)
trap 'rm -rf "${WORK}"' EXIT

echo "Fetching ${PORT_PATH} from freebsd-ports @ ${PORT_COMMIT} ..."
git init -q "${WORK}"
cd "${WORK}"
git remote add origin "${PORTS_REPO}"
git config core.sparseCheckout true
git sparse-checkout init --no-cone 2>/dev/null || true
git sparse-checkout set "${PORT_PATH}" 2>/dev/null || \
  printf '%s\n' "${PORT_PATH}/*" > .git/info/sparse-checkout
# GitHub allows fetching an arbitrary reachable SHA directly.
git fetch -q --depth 1 --filter=blob:none origin "${PORT_COMMIT}"
git checkout -q FETCH_HEAD

rm -rf "${DEST}"
mkdir -p "${DEST}"
cp "${PORT_PATH}"/patch-* "${DEST}/"

COUNT=$(ls -1 "${DEST}" | grep -c '^patch-' || true)
echo "Vendored ${COUNT} patch files into build/freebsd/patches/"
echo "Pinned commit: ${PORT_COMMIT}"
echo "Remember to refresh build/freebsd/MANIFEST if the commit/version changed."
