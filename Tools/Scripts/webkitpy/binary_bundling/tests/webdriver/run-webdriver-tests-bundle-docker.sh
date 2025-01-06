#!/bin/bash

function error_help() {
    echo "ERROR: ${@}"
    echo "Use: ${0} --platform {gtk,wpe} /path/to/MiniBrowser-bundle.tar.xz" 1>&2
    exit 1
}

[[ "${#}" -ge 2 ]] || error_help "Not enough parameters"
if [[ "${1}" == "--platform=gtk" ]] || [[ "${1}" == "--platform=wpe" ]]; then
    PORT="$(echo ${1}|cut -d= -f2)"
elif [[ "${1}" == "--platform" ]]; then
    if [[ "${2}" == "gtk" ]] || [[ "${2}" == "wpe" ]]; then
        PORT="${2}"
        shift 1
    else
        error_help "Unknown platform: ${2}"
    fi
else
    error_help "Unexpected parameter: ${1}"
fi

[[ -f "${2}" ]] || error_help "Bundle file does not exist: ${2}"
echo "${2}" | grep -qE "\.tar\.xz$" || error_help "Bundle file does not end in .tar.xz: ${2}"
FBUNDLE="$(readlink -f ${2})"

MYDIR="$(dirname $(readlink -f ${0}))"

TESTINITSCRIPT="install_deps_docker_start_test.sh"

set -eux

DISTROS_TO_TEST=(
    alpine:latest
    amazonlinux:latest
    archlinux:latest
    debian:11
    debian:12
    debian:testing
    fedora:latest
    gentoo/stage3:latest
    mageia:latest
    nixos/nix:latest
    rockylinux:9
    ubuntu:20.04
    ubuntu:22.04
    ubuntu:24.04
    ubuntu:latest
)

test -x "${MYDIR}/${TESTINITSCRIPT}"
HOSTNAME="$(hostname)"

I=0
for DISTRO in ${DISTROS_TO_TEST[@]}; do
    echo "[INFO] Start testing on distro: ${DISTRO}"
    docker pull "${DISTRO}"
    docker run --init --rm \
                -v "${MYDIR}:/testdata:ro" \
                -v "${FBUNDLE}:/testbundle.tar.xz:ro" \
                -v "/tmp/.X11-unix:/tmp/.X11-unix" \
                -v "${XAUTHORITY}:${XAUTHORITY}" \
                -e "XAUTHORITY=${XAUTHORITY}" \
                -e "DISPLAY=${DISPLAY}" \
                -h "${HOSTNAME}" \
                "${DISTRO}" \
                "/testdata/${TESTINITSCRIPT}" "${PORT}"
    echo "[PASS $((++I))/${#DISTROS_TO_TEST[@]}] Finish testing on distro: ${DISTRO}"
done

echo "[PASS][ALL OK] Bundle passed tests on all this ${#DISTROS_TO_TEST[@]} distros: $(echo ${DISTROS_TO_TEST[@]}|sed 's/ /, /g')"
exit 0
