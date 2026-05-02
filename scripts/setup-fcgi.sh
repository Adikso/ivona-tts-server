#!/bin/sh
set -eu

PREFIX="${1:-.fcgi-mingw}"
mkdir -p "$PREFIX"
PREFIX_ABS=$(cd "$PREFIX" && pwd)

if [ -f "$PREFIX_ABS/lib/libfcgi.a" ]; then
    echo "libfcgi already built at $PREFIX_ABS"
    exit 0
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

echo "==> cloning fcgi2"
git clone --depth=1 https://github.com/FastCGI-Archives/fcgi2 "$WORK/fcgi2" >/dev/null 2>&1

echo "==> configuring (mingw)"
cd "$WORK/fcgi2"
./autogen.sh >/dev/null 2>&1
./configure --host=x86_64-w64-mingw32 --prefix="$PREFIX_ABS" \
            --disable-shared --enable-static \
            CFLAGS="-O2 -Wno-error=incompatible-pointer-types" \
            >/dev/null

echo "==> building"
make -j"$(nproc 2>/dev/null || echo 2)" >/dev/null

echo "==> installing to $PREFIX_ABS"
make install >/dev/null
echo "done."
