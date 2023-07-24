/* Copyright (c) 2021, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

use std::env;
use std::path::Path;
use std::path::PathBuf;

fn get_bssl_build_dir() -> PathBuf {
    println!("cargo:rerun-if-env-changed=BORINGSSL_BUILD_DIR");
    if let Some(build_dir) = env::var_os("BORINGSSL_BUILD_DIR") {
        return PathBuf::from(build_dir);
    }

    let crate_dir = env::var_os("CARGO_MANIFEST_DIR").unwrap();
    return Path::new(&crate_dir).join("../../build");
}

fn main() {
    let bssl_build_dir = get_bssl_build_dir();
    let bssl_sys_build_dir = bssl_build_dir.join("rust/bssl-sys");
    let target = env::var("TARGET").unwrap();

    // Find the bindgen generated target platform bindings file and set BINDGEN_RS_FILE
    let bindgen_file = bssl_sys_build_dir.join(format!("wrapper_{}.rs", target));
    println!("cargo:rustc-env=BINDGEN_RS_FILE={}", bindgen_file.display());

    // Statically link libraries.
    println!(
        "cargo:rustc-link-search=native={}",
        bssl_build_dir.join("crypto").display()
    );
    println!("cargo:rustc-link-lib=static=crypto");

    println!(
        "cargo:rustc-link-search=native={}",
        bssl_build_dir.join("ssl").display()
    );
    println!("cargo:rustc-link-lib=static=ssl");

    println!(
        "cargo:rustc-link-search=native={}",
        bssl_sys_build_dir.display()
    );
    println!("cargo:rustc-link-lib=static=rust_wrapper");
}
