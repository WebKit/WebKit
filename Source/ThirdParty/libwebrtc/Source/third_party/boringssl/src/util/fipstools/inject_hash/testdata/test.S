# Copyright 2026 The BoringSSL Authors
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

# To regenerate test.o, run:
#   as -o util/fipstools/inject_hash/testdata/test.o util/fipstools/inject_hash/testdata/test.S
# or:
#   clang -c -o util/fipstools/inject_hash/testdata/test.o util/fipstools/inject_hash/testdata/test.S

	.text
.globl some_text_symbol_before
some_text_symbol_before:
	.byte 0x01, 0x02, 0x03, 0x04

.globl BORINGSSL_bcm_text_start
BORINGSSL_bcm_text_start:
	.byte 0xef, 0xbe, 0xad, 0xde

.globl BORINGSSL_bcm_text_end
BORINGSSL_bcm_text_end:
	.byte 0x11, 0x22, 0x33, 0x44

.globl some_text_symbol_after
some_text_symbol_after:
	.byte 0x55, 0x66, 0x77, 0x88

.section .rodata
.globl some_rodata_symbol_before
some_rodata_symbol_before:
	.byte 0x01, 0x02, 0x03, 0x04

.globl BORINGSSL_bcm_rodata_start
BORINGSSL_bcm_rodata_start:
	.byte 0xde, 0xad, 0xbe, 0xef

.globl BORINGSSL_bcm_rodata_end
BORINGSSL_bcm_rodata_end:
	.byte 0x11, 0x22, 0x33, 0x44

.globl some_rodata_symbol_after
some_rodata_symbol_after:
	.byte 0x55, 0x66, 0x77, 0x88

.globl BORINGSSL_bcm_text_hash
BORINGSSL_bcm_text_hash:
	.byte 0xae, 0x2c, 0xea, 0x2a, 0xbd, 0xa6, 0xf3, 0xec
	.byte 0x97, 0x7f, 0x9b, 0xf6, 0x94, 0x9a, 0xfc, 0x83
	.byte 0x68, 0x27, 0xcb, 0xa0, 0xa0, 0x9f, 0x6b, 0x6f
	.byte 0xde, 0x52, 0xcd, 0xe2, 0xcd, 0xff, 0x31, 0x80

.data
.globl some_data_symbol
some_data_symbol:
	.quad 0xdeadbeef
