#!/bin/sh
# Note: keep this script POSIX-shell compatible as some
# distros like Alpine do not ship by default with bash
set -eux

# Determine current distro
if [ -f /etc/os-release ]; then
    CURRENT_DISTRO="$(grep ^ID= /etc/os-release | cut -d= -f2 | sed 's/"//g' | tr '[:upper:]' '[:lower:]')"
elif [ -f /etc/nix/nix.conf -a -d /nix/store ]; then
    CURRENT_DISTRO="nixos"
else
    echo "Can not determine current distro type"
    exit 1
fi

export PYTHONUNBUFFERED=1

# Install needed packages
case "${CURRENT_DISTRO}" in
    alpine)
        apk update
        apk upgrade
        apk add py3-pip py3-pillow py3-numpy
        python3 -m pip config set global.break-system-packages true
        pip3 install selenium==4.24.0
    ;;
    ubuntu|debian)
        export DEBIAN_FRONTEND=noninteractive
        apt update
        apt upgrade -y
        apt install -y python3-pil python3-numpy python3-pip
        python3 -m pip config set global.break-system-packages true
        pip3 install selenium==4.24.0
    ;;
    fedora|amzn)
        dnf -y update
        dnf -y upgrade
        dnf -y install  python3-pip python3-pillow python3-numpy tar xz
        python3 -m pip config set global.break-system-packages true
        pip3 install selenium==4.24.0
    ;;
    rocky)
        dnf -y update
        dnf -y upgrade
        dnf -y install python3-pip tar xz
        python3 -m pip config set global.break-system-packages true
        pip3 install selenium==4.24.0 pillow numpy
    ;;
    arch)
        pacman --noconfirm -Syu
        pacman --noconfirm -Syu python-pip python-pillow python-numpy
        python3 -m pip config set global.break-system-packages true
        pip3 install selenium==4.24.0
    ;;
    mageia)
        urpmi --auto --auto-update
        urpmi --auto python3-pip tar xz python3-pillow python3-numpy
        python3 -m pip config set global.break-system-packages true
        pip3 install selenium==4.24.0
    ;;
    nixos)
        nix-env -iA nixpkgs.python3Packages.numpy nixpkgs.python3Packages.pillow nixpkgs.python3Packages.pip nixpkgs.xz
        pip3 install --break-system-packages selenium==4.24.0
    ;;
    gentoo)
        emaint --auto sync
        emerge dev-python/pip
        python3 -m pip config set global.break-system-packages true
        pip3 install selenium==4.24.0 pillow numpy
    ;;
    *)
        echo "Unknown distro: ${CURRENT_DISTRO}"
        exit 1
    ;;
esac

# prepare for testing
mkdir /testbundle
tar xfav /testbundle.tar.xz -C /testbundle
echo "Starting tests on distro: ${CURRENT_DISTRO}"
[ -f /etc/os-release ] && cat /etc/os-release
# run the tests
if [ "${CURRENT_DISTRO}" = "nixos" ]; then
    nix-shell -p python3 python3Packages.pip python3Packages.pillow python3Packages.numpy --run "/testdata/test_webdriver_bundle.py --platform=$1 /testbundle"
else
    /testdata/test_webdriver_bundle.py "--platform=$1" /testbundle
fi
