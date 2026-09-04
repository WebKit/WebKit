#!/bin/sh
# Note: keep this script POSIX-shell compatible as some
# distros like Alpine do not ship by default with bash
set -eux

# Determine current distro
if [ -f /etc/os-release ]; then
    CURRENT_DISTRO="$(grep ^ID= /etc/os-release | cut -d= -f2 | tr -d "'\"" | tr '[:upper:]' '[:lower:]')"
elif [ -f /etc/nix/nix.conf -a -d /nix/store ]; then
    CURRENT_DISTRO="nixos"
else
    echo "Can not determine current distro type"
    exit 1
fi

export PYTHONUNBUFFERED=1

HEADLESS=""
if [ "${1}" = "--headless" ]; then
    HEADLESS="--headless"
    shift 1
fi

# Unfortunately it is not unusual that some of this distros randomly fail to setup
# the initial environment for running the webdriver tests with python/selenium.
# To deal with that, this script tells the caller the reason of the failure via a
# special exit code. Exit code of $BUNDLETEST_SETUP_FAILURE_RC means a failure while
# setting up the distro env (install deps), which is tolerable, any other non-zero
# exit code means a fatal failure while running the webdriver/minibrowser tests.
BUNDLETEST_SETUP_FAILURE_RC=42
bundletest_on_exit() {
    rc=$?
    [ "${rc}" -ne 0 ] || return 0
    echo "[BUNDLETEST][RESULT] ENV_SETUP_FAILED rc=${rc} distro=${CURRENT_DISTRO:-unknown}"
    exit "${BUNDLETEST_SETUP_FAILURE_RC}"
}
trap bundletest_on_exit EXIT

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
        # Bypass emerge for installing pip, it has been causing issues when there are python migrations on Gentoo.
        rm -f /usr/lib/python*/EXTERNALLY-MANAGED
        python3 -m ensurepip --upgrade
        pip3 install selenium==4.24.0 pillow numpy
    ;;
    *)
        echo "Unknown distro: ${CURRENT_DISTRO}"
        trap - EXIT
        exit 1
    ;;
esac

# Distro initial setup done: disarm the setup-failure handler and start the tests. From here any failure
# propagates the real exit code unchanged, so the caller treats it as a fatal test/launch failure.
trap - EXIT
mkdir /testbundle
tar xfa /testbundle.tar.xz -C /testbundle
echo "Starting tests on distro: ${CURRENT_DISTRO}"
[ -f /etc/os-release ] && cat /etc/os-release
CMD="/testdata/test_webdriver_bundle.py ${HEADLESS} --platform=$1 /testbundle"
if [ "${CURRENT_DISTRO}" = "nixos" ]; then
    exec nix-shell -p python3 python3Packages.pip python3Packages.pillow python3Packages.numpy --run "${CMD}"
else
    exec ${CMD}
fi
