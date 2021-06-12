#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) 2020, Google Inc.
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

"""This script translates JSON test vectors to BoringSSL's "FileTest" format.

Usage: translate_test_vectors.py TEST_VECTORS_JSON_FILE

The TEST_VECTORS_JSON_FILE is expected to come from the HPKE reference
implementation at https://github.com/cisco/go-hpke. The output file is
hardcoded as "hpke_test_vectors.txt".
"""

import collections
import json
import sys

HPKE_MODE_BASE = 0
HPKE_MODE_PSK = 1
HPKE_DHKEM_X25519_SHA256 = 0x0020
HPKE_HKDF_SHA256 = 0x0001
HPKE_AEAD_EXPORT_ONLY = 0xffff


def read_test_vectors_and_generate_code(json_file_in_path, test_file_out_path):
  """Translates JSON test vectors into BoringSSL's FileTest language.

    Args:
      json_file_in_path: Path to the JSON test vectors file.
      test_file_out_path: Path to output file.
  """

  # Load the JSON file into |test_vecs|.
  with open(json_file_in_path) as file_in:
    test_vecs = json.load(file_in)

  lines = []
  for test in test_vecs:
    # Filter out test cases that we don't use.
    if (test["mode"] != HPKE_MODE_BASE or
        test["kem_id"] != HPKE_DHKEM_X25519_SHA256 or
        test["aead_id"] == HPKE_AEAD_EXPORT_ONLY or
        test["kdf_id"] != HPKE_HKDF_SHA256):
      continue

    keys = ["mode", "kdf_id", "aead_id", "info", "skRm", "skEm", "pkRm", "pkEm"]

    if test["mode"] == HPKE_MODE_PSK:
      keys.append("psk")
      keys.append("psk_id")

    for key in keys:
      lines.append("{} = {}".format(key, str(test[key])))

    for i, enc in enumerate(test["encryptions"]):
      lines.append("# encryptions[{}]".format(i))
      for key in ("aad", "ciphertext", "plaintext"):
        lines.append("{} = {}".format(key, str(enc[key])))

    for i, exp in enumerate(test["exports"]):
      lines.append("# exports[{}]".format(i))
      for key in ("exporter_context", "L", "exported_value"):
        lines.append("{} = {}".format(key, str(exp[key])))

    lines.append("")

  with open(test_file_out_path, "w") as file_out:
    file_out.write("\n".join(lines))


def main(argv):
  if len(argv) != 2:
    print(__doc__)
    sys.exit(1)

  read_test_vectors_and_generate_code(argv[1], "hpke_test_vectors.txt")


if __name__ == "__main__":
  main(sys.argv)
