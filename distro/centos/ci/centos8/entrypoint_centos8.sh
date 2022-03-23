#!/usr/bin/env bash

set -euo pipefail
set -x

separator() {
    echo -e "\n\n\n\n\n"
}

# base
cd /etc/yum.repos.d/
sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*
sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
yum update -y
yum groupinstall -y 'Development Tools'

# dependencies
yum install -y \
    desktop-file-utils \
    gettext \
    gtk2-devel\
    gtk3-devel \
    libXtst-devel \
    qt5-qtbase-devel \
    qt5-qtbase-private-devel \


set +x; separator; separator; separator; set -x

# enter GitHub workspace directory path
cd "$GITHUB_WORKSPACE"

./configure \
    --disable-gtk2-im-module \
    --disable-gtk3-im-module \
    --disable-qt6-immodule \
    --prefix="$PWD/build" \
    --qt5-im-module-path=/usr/lib/qt/plugins/platforminputcontexts/

make
