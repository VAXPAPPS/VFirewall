#!/bin/bash

# Exit on error
set -e

# Package details
PKG_NAME="vaxp-firewall"
VERSION="0.1.0"
ARCH="amd64"
MAINTAINER="Vaxp <maintainer@example.com>"
DESCRIPTION="Configure Uncomplicated Firewall (UFW) via a modern GTK4 interface."

# Working directories
BUILD_DIR="build_deb_tmp"
PKG_DIR="${PKG_NAME}_${VERSION}_${ARCH}"

echo ">>> Cleaning up previous builds..."
rm -rf "$BUILD_DIR"
rm -rf "$PKG_DIR"
rm -f "${PKG_DIR}.deb"

echo ">>> Setting up Meson build system..."
meson setup "$BUILD_DIR" --prefix=/usr --buildtype=release

echo ">>> Compiling project..."
meson compile -C "$BUILD_DIR"

echo ">>> Installing to package directory..."
DESTDIR="$(pwd)/$PKG_DIR" meson install -C "$BUILD_DIR"

# Note: The executable compiled by Meson is 'VFirewall' but 'vaxp-firewall.desktop' 
# expects 'vaxp-firewall' as the executable. We rename it here to ensure it works.
if [ -f "$PKG_DIR/usr/bin/VFirewall" ]; then
    echo ">>> Renaming executable to match desktop file..."
    mv "$PKG_DIR/usr/bin/VFirewall" "$PKG_DIR/usr/bin/vaxp-firewall"
fi

echo ">>> Creating DEBIAN directory..."
mkdir -p "$PKG_DIR/DEBIAN"

echo ">>> Generating control file..."
cat <<EOF > "$PKG_DIR/DEBIAN/control"
Package: $PKG_NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: $MAINTAINER
Description: $DESCRIPTION
Depends: libc6, libglib2.0-0, libgtk-4-1, libnotify4, ufw, polkitd | policykit-1
EOF

echo ">>> Building Debian package..."
dpkg-deb --build "$PKG_DIR"

echo ">>> Cleaning up temporary directory..."
rm -rf "$BUILD_DIR"
rm -rf "$PKG_DIR"

echo ">>> Done! Package created: ${PKG_DIR}.deb"
