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

// Keep in sync with the list in include/openssl/opensslconf.h
const OSSL_CONF_DEFINES: &[&str] = &[
    "OPENSSL_NO_ASYNC",
    "OPENSSL_NO_BF",
    "OPENSSL_NO_BLAKE2",
    "OPENSSL_NO_BUF_FREELISTS",
    "OPENSSL_NO_CAMELLIA",
    "OPENSSL_NO_CAPIENG",
    "OPENSSL_NO_CAST",
    "OPENSSL_NO_CMS",
    "OPENSSL_NO_COMP",
    "OPENSSL_NO_CT",
    "OPENSSL_NO_DANE",
    "OPENSSL_NO_DEPRECATED",
    "OPENSSL_NO_DGRAM",
    "OPENSSL_NO_DYNAMIC_ENGINE",
    "OPENSSL_NO_EC_NISTP_64_GCC_128",
    "OPENSSL_NO_EC2M",
    "OPENSSL_NO_EGD",
    "OPENSSL_NO_ENGINE",
    "OPENSSL_NO_GMP",
    "OPENSSL_NO_GOST",
    "OPENSSL_NO_HEARTBEATS",
    "OPENSSL_NO_HW",
    "OPENSSL_NO_IDEA",
    "OPENSSL_NO_JPAKE",
    "OPENSSL_NO_KRB5",
    "OPENSSL_NO_MD2",
    "OPENSSL_NO_MDC2",
    "OPENSSL_NO_OCB",
    "OPENSSL_NO_OCSP",
    "OPENSSL_NO_RC2",
    "OPENSSL_NO_RC5",
    "OPENSSL_NO_RFC3779",
    "OPENSSL_NO_RIPEMD",
    "OPENSSL_NO_RMD160",
    "OPENSSL_NO_SCTP",
    "OPENSSL_NO_SEED",
    "OPENSSL_NO_SM2",
    "OPENSSL_NO_SM3",
    "OPENSSL_NO_SM4",
    "OPENSSL_NO_SRP",
    "OPENSSL_NO_SSL_TRACE",
    "OPENSSL_NO_SSL2",
    "OPENSSL_NO_SSL3",
    "OPENSSL_NO_SSL3_METHOD",
    "OPENSSL_NO_STATIC_ENGINE",
    "OPENSSL_NO_STORE",
    "OPENSSL_NO_WHIRLPOOL",
];

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

    println!("cargo:conf={}", OSSL_CONF_DEFINES.join(","));
}
