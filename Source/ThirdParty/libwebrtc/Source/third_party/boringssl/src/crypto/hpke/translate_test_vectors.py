#!/usr/bin/env python3
# coding=utf-8
# Copyright 2020 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This script translates JSON test vectors to BoringSSL's "FileTest" format.

Usage: translate_test_vectors.py

It expects two JSON files:
- test-vectors.json, from [TestVectors] in RFC 9180
- test-vectors-pq.json, from [TestVectors] in draft-ietf-hpke-pq

The output is written to hpke_test_vectors.txt and hpke_test_vectors_pq.txt,
respectively.
"""

import collections
import json
import sys

HPKE_MODE_BASE = 0
HPKE_MODE_PSK = 1
HPKE_MODE_AUTH = 2

HPKE_DHKEM_P256_SHA256 = 0x0010
HPKE_DHKEM_X25519_SHA256 = 0x0020
HPKE_MLKEM768 = 0x0041
HPKE_MLKEM1024 = 0x0042
HPKE_MLKEM768_X25519 = 0x647a

HPKE_HKDF_SHA256 = 0x0001
HPKE_HKDF_SHA384 = 0x0002

HPKE_AEAD_EXPORT_ONLY = 0xffff

SUPPORTED_HPKE_KDFS = (
  HPKE_HKDF_SHA256,
  HPKE_HKDF_SHA384,
)

SUPPORTED_HPKE_KEMS = (
  HPKE_DHKEM_X25519_SHA256,
  HPKE_DHKEM_P256_SHA256,
  HPKE_MLKEM768,
  HPKE_MLKEM1024,
  HPKE_MLKEM768_X25519,
)

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
    if (test["mode"] not in (HPKE_MODE_BASE, HPKE_MODE_AUTH) or
        test["kem_id"] not in SUPPORTED_HPKE_KEMS or
        test["aead_id"] == HPKE_AEAD_EXPORT_ONLY or
        test["kdf_id"] not in SUPPORTED_HPKE_KDFS):
      continue

    keys = ["mode", "kem_id", "kdf_id", "aead_id", "info", "skRm", "pkRm", "enc", "ikmR"]

    if test["mode"] == HPKE_MODE_AUTH:
      keys.append("pkSm")
      keys.append("skSm")

    for key in keys:
      lines.append(f"{key} = {test[key]}".rstrip())

    if test["kem_id"] == HPKE_DHKEM_X25519_SHA256:
      # For X25519, our setup_sender_with_seed API uses the ephemeral secret key
      # as the seed. Others use the ikmE value.
      lines.append(f"sender_seed = {test['skEm']}".rstrip())
    else:
      lines.append(f"sender_seed = {test['ikmE']}".rstrip())

    for i, enc in enumerate(test["encryptions"]):
      lines.append("# encryptions[{}]".format(i))
      for key in ("aad", "ct", "pt"):
        lines.append(f"{key} = {enc[key]}".rstrip())

    for i, exp in enumerate(test["exports"]):
      lines.append("# exports[{}]".format(i))
      for key in ("exporter_context", "L", "exported_value"):
        lines.append(f"{key} = {exp[key]}".rstrip())

    lines.append("")

  with open(test_file_out_path, "w") as file_out:
    file_out.write("\n".join(lines))


def main(argv):
  if len(argv) != 1:
    print(__doc__)
    sys.exit(1)

  read_test_vectors_and_generate_code(
      "test-vectors.json", "hpke_test_vectors.txt")
  read_test_vectors_and_generate_code(
      "test-vectors-pq.json", "hpke_test_vectors_pq.txt")


if __name__ == "__main__":
  main(sys.argv)
