#!/bin/bash
# Copyright (c) 2016, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

set -ex

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 fuzzer_mode_build_dir no_fuzzer_mode_build_dir"
  exit 1
fi

fuzzer_mode_build_dir=$1
no_fuzzer_mode_build_dir=$2


# Sanity-check the build directories.

if ! grep -q '^FUZZ:' "$fuzzer_mode_build_dir/CMakeCache.txt"; then
  echo "$fuzzer_mode_build_dir was not built with -DFUZZ=1"
  exit 1
fi

if grep -q '^NO_FUZZER_MODE:' "$fuzzer_mode_build_dir/CMakeCache.txt"; then
  echo "$fuzzer_mode_build_dir was built with -DNO_FUZZER_MODE=1"
  exit 1
fi

if ! grep -q '^FUZZ:' "$no_fuzzer_mode_build_dir/CMakeCache.txt"; then
  echo "$no_fuzzer_mode_build_dir was not built with -DFUZZ=1"
  exit 1
fi

if ! grep -q '^NO_FUZZER_MODE:' "$no_fuzzer_mode_build_dir/CMakeCache.txt"; then
  echo "$no_fuzzer_mode_build_dir was not built with -DNO_FUZZER_MODE=1"
  exit 1
fi


# Sanity-check the current working directory.

assert_directory() {
  if [[ ! -d $1 ]]; then
    echo "$1 not found."
    exit 1
  fi
}

assert_directory client_corpus
assert_directory client_corpus_no_fuzzer_mode
assert_directory server_corpus
assert_directory server_corpus_no_fuzzer_mode
assert_directory dtls_client_corpus
assert_directory dtls_server_corpus


# Gather new transcripts. Ignore errors in running the tests.

fuzzer_mode_shim=$(readlink -f "$fuzzer_mode_build_dir/ssl/test/bssl_shim")
no_fuzzer_mode_shim=$(readlink -f \
    "$no_fuzzer_mode_build_dir/ssl/test/bssl_shim")

fuzzer_mode_transcripts=$(mktemp -d '/tmp/boringssl-transcript.XXXXXX')
no_fuzzer_mode_transcripts=$(mktemp -d '/tmp/boringssl-transcript.XXXXXX')

echo Recording fuzzer-mode transcripts
(cd ../ssl/test/runner/ && go test \
    -shim-path "$fuzzer_mode_shim" \
    -transcript-dir "$fuzzer_mode_transcripts" \
    -fuzzer \
    -deterministic) || true

echo Recording non-fuzzer-mode transcripts
(cd ../ssl/test/runner/ && go test \
    -shim-path "$no_fuzzer_mode_shim" \
    -transcript-dir "$no_fuzzer_mode_transcripts" \
    -deterministic) || true


# Minimize the existing corpora.

minimize_corpus() {
  local fuzzer="$1"
  local corpus="$2"

  echo "Minimizing ${corpus}"
  mv "$corpus" "${corpus}_old"
  mkdir "$corpus"
  "$fuzzer" -max_len=50000 -merge=1 "$corpus" "${corpus}_old"
  rm -Rf "${corpus}_old"
}

minimize_corpus "$fuzzer_mode_build_dir/fuzz/client" client_corpus
minimize_corpus "$fuzzer_mode_build_dir/fuzz/server" server_corpus
minimize_corpus "$no_fuzzer_mode_build_dir/fuzz/client" client_corpus_no_fuzzer_mode
minimize_corpus "$no_fuzzer_mode_build_dir/fuzz/server" server_corpus_no_fuzzer_mode
minimize_corpus "$fuzzer_mode_build_dir/fuzz/dtls_client" dtls_client_corpus
minimize_corpus "$fuzzer_mode_build_dir/fuzz/dtls_server" dtls_server_corpus


# Incorporate the new transcripts.

"$fuzzer_mode_build_dir/fuzz/client" -max_len=50000 -merge=1 client_corpus "${fuzzer_mode_transcripts}/tls/client"
"$fuzzer_mode_build_dir/fuzz/server" -max_len=50000 -merge=1 server_corpus "${fuzzer_mode_transcripts}/tls/server"
"$no_fuzzer_mode_build_dir/fuzz/client" -max_len=50000 -merge=1 client_corpus_no_fuzzer_mode "${no_fuzzer_mode_transcripts}/tls/client"
"$no_fuzzer_mode_build_dir/fuzz/server" -max_len=50000 -merge=1 server_corpus_no_fuzzer_mode "${no_fuzzer_mode_transcripts}/tls/server"
"$fuzzer_mode_build_dir/fuzz/dtls_client" -max_len=50000 -merge=1 dtls_client_corpus "${fuzzer_mode_transcripts}/dtls/client"
"$fuzzer_mode_build_dir/fuzz/dtls_server" -max_len=50000 -merge=1 dtls_server_corpus "${fuzzer_mode_transcripts}/dtls/server"
