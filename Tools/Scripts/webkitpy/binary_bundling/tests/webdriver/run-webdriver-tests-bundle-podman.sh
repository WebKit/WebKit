#!/bin/bash

function error_help() {
    echo "ERROR: ${@}"
    echo "Use: ${0} --platform {gtk,wpe} [--headless] /path/to/MiniBrowser-bundle.tar.xz" 1>&2
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
HEADLESS=""
if [[ "${2}" == "--headless" ]]; then
    HEADLESS="--headless"
    shift 1
fi
[[ -f "${2}" ]] || error_help "Bundle file does not exist: ${2}"
echo "${2}" | grep -qE "\.tar\.xz$" || error_help "Bundle file does not end in .tar.xz: ${2}"
FBUNDLE="$(readlink -f ${2})"

MYDIR="$(dirname $(readlink -f ${0}))"

TESTINITSCRIPT="install_deps_podman_start_test.sh"
PODMAN_DISPLAY_BIND=""
[[ -n "${DISPLAY}" ]] && PODMAN_DISPLAY_BIND+="-v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=${DISPLAY} "
[[ -n "${XAUTHORITY}" ]] && PODMAN_DISPLAY_BIND+="-v ${XAUTHORITY}:${XAUTHORITY} -e XAUTHORITY=${XAUTHORITY} "
[[ -n "${WAYLAND_DISPLAY}" ]] && PODMAN_DISPLAY_BIND+="-v ${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}:/tmp/${WAYLAND_DISPLAY} -e XDG_RUNTIME_DIR=/tmp -e WAYLAND_DISPLAY=${WAYLAND_DISPLAY} "

set -eux

# Exit code used to signal a failure that happened while setting up the distro (installing deps)
BUNDLETEST_SETUP_FAILURE_RC=42

# Max number of distros that are allowed to fail "environment setup" (deps install or image pull)
MAX_ENV_SETUP_FAILURES="${MAX_ENV_SETUP_FAILURES:-2}"

DISTROS_TO_TEST=(
    alpine:latest
    amazonlinux:latest
    archlinux:latest
    debian:11
    debian:12
    debian:13
    debian:testing
    fedora:latest
    gentoo/stage3:latest
    mageia:latest
    nixos/nix:latest
    rockylinux:9
    ubuntu:20.04
    ubuntu:22.04
    ubuntu:24.04
    ubuntu:26.04
    ubuntu:latest
)

if [[ ! -x "${MYDIR}/${TESTINITSCRIPT}" ]]; then
    echo "[BUNDLETEST][FATAL] Test init script not found or not executable: ${MYDIR}/${TESTINITSCRIPT}" 1>&2
    exit 1
fi
HOSTNAME="$(hostname)"

I=0
N_ENV_FAILURES=0
ENV_FAILED_DISTROS=()
for DISTRO in ${DISTROS_TO_TEST[@]}; do
    I=$((I + 1))
    echo "[INFO] Start testing on distro: ${DISTRO} (${I}/${#DISTROS_TO_TEST[@]})"
    IMAGE="docker.io/${DISTRO}"

    # Image pull failures are an environment/registry problem (network, mirror,
    # rate limit), not a bundle problem -> treat as a tolerable env failure.
    if ! podman pull "${IMAGE}"; then
        echo "[BUNDLETEST][ENV-FAIL] Could not pull image for ${DISTRO} (registry/network issue)"
        N_ENV_FAILURES=$((N_ENV_FAILURES + 1))
        ENV_FAILED_DISTROS+=("${DISTRO}(pull)")
        continue
    fi

    RC=0
    podman run --init --rm \
                -v "${MYDIR}:/testdata:ro" \
                -v "${FBUNDLE}:/testbundle.tar.xz:ro" \
                ${PODMAN_DISPLAY_BIND} \
                -h "${HOSTNAME}" \
                "${IMAGE}" \
                "/testdata/${TESTINITSCRIPT}" ${HEADLESS} "${PORT}" || RC=$?

    if [[ "${RC}" -eq 0 ]]; then
        echo "[PASS ${I}/${#DISTROS_TO_TEST[@]}] Finish testing on distro: ${DISTRO}"
    elif [[ "${RC}" -eq "${BUNDLETEST_SETUP_FAILURE_RC}" ]]; then
        # Tolerable initial distro env setup (deps install) failure up to configured threshold.
        echo "[BUNDLETEST][ENV-FAIL] Environment setup (deps install) failed on ${DISTRO}"
        N_ENV_FAILURES=$((N_ENV_FAILURES + 1))
        ENV_FAILED_DISTROS+=("${DISTRO}")
    else
        # Any other non-zero code is the test runner's own exit (number of failed tests)
        # or a crash/OOM during the tests. Real problem with the bundle -> always fatal.
        echo "[BUNDLETEST][TEST-FAIL][FATAL] MiniBrowser/webdriver tests failed on ${DISTRO} (exit code ${RC})" 1>&2
        exit 1
    fi
done

if [[ "${N_ENV_FAILURES}" -gt "${MAX_ENV_SETUP_FAILURES}" ]]; then
    echo "[BUNDLETEST][FATAL] Too many distros failed environment setup: ${N_ENV_FAILURES} > ${MAX_ENV_SETUP_FAILURES} allowed." 1>&2
    echo "[BUNDLETEST][FATAL] Distros with env setup failures: ${ENV_FAILED_DISTROS[*]}" 1>&2
    exit 1
fi

N_TESTED=$(( ${#DISTROS_TO_TEST[@]} - N_ENV_FAILURES ))
if [[ "${N_ENV_FAILURES}" -gt 0 ]]; then
    echo "[BUNDLETEST][WARN] ${N_ENV_FAILURES} distro(s) had NON-FATAL environment setup failures (within the ${MAX_ENV_SETUP_FAILURES} allowed): $(echo "${ENV_FAILED_DISTROS[@]}" | sed 's/ /, /g')"
    echo "[PASS][OK] Bundle passed tests on ${N_TESTED} of ${#DISTROS_TO_TEST[@]} distros (${N_ENV_FAILURES} skipped due to env setup failure)."
else
    echo "[PASS][ALL OK] Bundle passed tests on all these ${#DISTROS_TO_TEST[@]} distros: $(echo ${DISTROS_TO_TEST[@]} | sed 's/ /, /g')"
fi
exit 0
