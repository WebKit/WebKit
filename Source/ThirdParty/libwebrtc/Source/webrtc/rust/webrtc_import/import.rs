// Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

//! WebRTC Rust Portability Import Proc-Macro Wrapper.
//!
//! This crate provides the `import!` macro which serves as a layer of
//! indirection over Chromium's `chromium::import!`. It rewrites source-relative
//! GN paths dynamically based on whether WebRTC is built standalone or embedded
//! inside a vendor directory tree (like `//third_party/webrtc`).

extern crate proc_macro;
use proc_macro::TokenStream;
use quote::quote;
use syn::parse::{Parse, ParseStream};
use syn::punctuated::Punctuated;
use syn::{parse_macro_input, Ident, LitStr, Token, Visibility};

mod path_utils;

/// Represents a single dependency import line item inside the macro.
///
/// Example syntax: `pub "//api/units:time_delta_rs" as time_delta;`
struct ImportItem {
    /// Optional visibility prefix (e.g., `pub`, `pub(crate)`).
    vis: Option<Visibility>,
    /// Quoted string literal containing the absolute or relative GN target
    /// label.
    path: LitStr,
    /// Optional custom crate rename alias (e.g., `as renamed_crate`).
    alias: Option<(Token![as], Ident)>,
}

impl Parse for ImportItem {
    /// Parses a single import entry matching: `[vis] "path" [as alias]`
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let vis = input.parse::<Visibility>().ok();
        let path = input.parse::<LitStr>()?;
        let alias = if input.peek(Token![as]) {
            Some((input.parse::<Token![as]>()?, input.parse::<Ident>()?))
        } else {
            None
        };
        Ok(ImportItem { vis, path, alias })
    }
}

/// Represents the full body of the macro invocation, containing
/// semicolon-separated entries.
struct ImportList {
    imports: Punctuated<ImportItem, Token![;]>,
}

impl Parse for ImportList {
    /// Parses a full stream of semicolon-terminated `ImportItem` entries.
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(ImportList { imports: Punctuated::parse_terminated(input)? })
    }
}

/// The entry point for `webrtc::import!`.
///
/// This proc-macro executes during host compile time. It scans the list of
/// absolute GN path strings, prepends the build context's `WEBRTC_GN_PREFIX`
/// (injected via GN `rustenv`), and forwards the rewritten token tree onto
/// Chromium's canonical `chromium::import!` macro.
#[proc_macro]
pub fn import(input: TokenStream) -> TokenStream {
    // Read the build root prefix provided by GN's `rustenv` definition.
    // Defaults to standalone root "//" if not set.
    let prefix = std::env::var("WEBRTC_GN_PREFIX").unwrap_or_else(|_| "//".to_string());
    let list = parse_macro_input!(input as ImportList);

    let mut adjusted_tokens = quote! {};

    for item in list.imports {
        let vis = item.vis;
        let path_str = item.path.value();

        // Delegate the string manipulation logic to the shared, unittested path_utils
        // module.
        let adjusted_path = path_utils::adjust_path(&prefix, &path_str);

        let new_path = LitStr::new(&adjusted_path, item.path.span());
        let alias_tokens = item.alias.map(|(as_tok, ident)| quote! { #as_tok #ident });

        // Reassemble the item with the new path literal token
        adjusted_tokens.extend(quote! {
            #vis #new_path #alias_tokens ;
        });
    }

    // Expand into a call to Chromium's macro backend, passing the modified paths.
    let expanded = quote! {
        ::chromium::import! { #adjusted_tokens }
    };
    expanded.into()
}
