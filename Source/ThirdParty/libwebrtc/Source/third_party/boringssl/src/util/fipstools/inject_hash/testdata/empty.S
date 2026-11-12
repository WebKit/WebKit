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

# To regenerate empty.o, run:
#   as -o util/fipstools/inject_hash/testdata/empty.o util/fipstools/inject_hash/testdata/empty.S
# or:
#   clang -c -o util/fipstools/inject_hash/testdata/empty.o util/fipstools/inject_hash/testdata/empty.S

	.text
.globl some_other_symbol
some_other_symbol:
	.byte 0x00
