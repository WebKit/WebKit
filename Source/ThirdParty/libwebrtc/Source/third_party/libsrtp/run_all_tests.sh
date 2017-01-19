#!/bin/bash

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

run_all_tests() {
  export LD_LIBRARY_PATH="${BASE}/.."  # libboringssl.so

  # rtwp_test.sh and rtpw_test_gcm.sh expect to run ./rtpw
  pushd "${BASE}"

  ./cipher_driver -v &&
    ./datatypes_driver -v &&
    ./dtls_srtp_driver &&
    ./kernel_driver -v &&
    ./rdbx_driver -v &&
    ./replay_driver -v &&
    ./roc_driver -v &&
    ./srtp_driver -v &&
    ./rtpw_test.sh &&
    ./rtpw_test_gcm.sh
}

(run_all_tests && echo PASSED) || echo FAILED
