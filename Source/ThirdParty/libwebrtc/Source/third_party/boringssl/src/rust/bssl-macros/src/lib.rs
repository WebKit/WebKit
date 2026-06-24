// Copyright 2026 The BoringSSL Authors
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


#![no_std]

#[doc(hidden)]
#[macro_export]
macro_rules! bssl_enum {
    (
        $(#[$attr:meta])*
        $vis:vis enum $name:ident : $repr:ty {
            $(
                $(#[$vattr:meta])*
                $item:ident = $e:expr
            ),* $(,)?
        }
    ) => {
        $(#[$attr])*
        #[repr($repr)]
        $vis enum $name {
            $(
                $(#[$vattr])*
                $item = $e,
            )*
        }

        impl ::core::convert::TryFrom<$repr> for $name {
            type Error = $repr;

            fn try_from(value: $repr) -> Result<Self, Self::Error> {
                $(
                    #[allow(non_upper_case_globals)]
                    const $item: $repr = $e;
                )*

                #[allow(non_upper_case_globals)]
                match value {
                    $(
                        $item => ::core::result::Result::Ok(Self::$item),
                    )*
                    _ => ::core::result::Result::Err(value),
                }
            }
        }
    };
}
