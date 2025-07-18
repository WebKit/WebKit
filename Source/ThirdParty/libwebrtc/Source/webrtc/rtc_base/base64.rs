/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

use base64::alphabet;
use base64::engine::general_purpose;
use base64::engine::DecodePaddingMode;
use base64::Engine;
use cxx::CxxString;
use std::pin::Pin;

#[cxx::bridge(namespace = "webrtc")]
mod ffi {
    #[repr(u8)]
    enum Base64DecodeOptions {
        kStrict,
        kForgiving,
    }

    extern "C++" {
        include!("rtc_base/base64.h");
        type Base64DecodeOptions;
    }

    extern "Rust" {
        fn rs_base64_encode(data: &[u8]) -> String;
        fn rs_base64_decode(
            data: &[u8],
            options: Base64DecodeOptions,
            output: Pin<&mut CxxString>,
        ) -> bool;
    }
}

fn rs_base64_encode(data: &[u8]) -> String {
    general_purpose::STANDARD.encode(data)
}

const FORGIVING_ENGINE: general_purpose::GeneralPurpose = general_purpose::GeneralPurpose::new(
    &alphabet::STANDARD,
    general_purpose::GeneralPurposeConfig::new()
        .with_decode_padding_mode(DecodePaddingMode::Indifferent),
);

fn rs_base64_decode(
    data: &[u8],
    options: ffi::Base64DecodeOptions,
    output: Pin<&mut CxxString>,
) -> bool {
    let result = match options {
        ffi::Base64DecodeOptions::kStrict => general_purpose::STANDARD.decode(data),
        ffi::Base64DecodeOptions::kForgiving => {
            let data_without_whitespace: Vec<u8> =
                data.iter().filter(|&c| !c.is_ascii_whitespace()).copied().collect();
            FORGIVING_ENGINE.decode(data_without_whitespace)
        }
        _ => unreachable!(),
    };

    match result {
        Ok(vec) => {
            output.push_bytes(vec.as_slice());
            true
        }
        Err(_) => false,
    }
}
