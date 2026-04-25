// Copyright 2023 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
use alloc::vec::Vec;

#[allow(clippy::expect_used, clippy::unwrap_used, clippy::indexing_slicing)]
pub(crate) fn decode_hex<const N: usize>(s: &str) -> [u8; N] {
    (0..s.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&s[i..i + 2], 16).expect("Invalid hex string"))
        .collect::<Vec<u8>>()
        .as_slice()
        .try_into()
        .unwrap()
}

#[allow(clippy::expect_used, clippy::unwrap_used, clippy::indexing_slicing)]
pub(crate) fn decode_hex_into_vec(s: &str) -> Vec<u8> {
    (0..s.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&s[i..i + 2], 16).expect("Invalid hex string"))
        .collect::<Vec<u8>>()
}
