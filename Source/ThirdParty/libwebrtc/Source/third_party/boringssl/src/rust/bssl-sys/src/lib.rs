#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use core::ffi::c_ulong;

// Wrap the bindgen output in a module and re-export it, so we can override it
// as needed.
mod bindgen {
    #[cfg(not(soong))]
    include!(env!("BINDGEN_RS_FILE"));
    // Soong, Android's build tool, does not support configuring environment
    // variables like other Rust build systems too. However, it does support
    // some hardcoded behavior with the OUT_DIR variable.
    #[cfg(soong)]
    include!(concat!(env!("OUT_DIR"), "/bssl_sys_bindings.rs"));
}
pub use bindgen::*;

// bindgen does not handle C constants correctly. See
// https://github.com/rust-lang/rust-bindgen/issues/923. Work around this bug by
// redefining some constants with the correct type. Once the bindgen bug has
// been fixed, remove this.
pub const ASN1_STRFLGS_ESC_2253: c_ulong = bindgen::ASN1_STRFLGS_ESC_2253 as c_ulong;
pub const ASN1_STRFLGS_ESC_CTRL: c_ulong = bindgen::ASN1_STRFLGS_ESC_CTRL as c_ulong;
pub const ASN1_STRFLGS_ESC_MSB: c_ulong = bindgen::ASN1_STRFLGS_ESC_MSB as c_ulong;
pub const ASN1_STRFLGS_ESC_QUOTE: c_ulong = bindgen::ASN1_STRFLGS_ESC_QUOTE as c_ulong;
pub const ASN1_STRFLGS_UTF8_CONVERT: c_ulong = bindgen::ASN1_STRFLGS_UTF8_CONVERT as c_ulong;
pub const ASN1_STRFLGS_IGNORE_TYPE: c_ulong = bindgen::ASN1_STRFLGS_IGNORE_TYPE as c_ulong;
pub const ASN1_STRFLGS_SHOW_TYPE: c_ulong = bindgen::ASN1_STRFLGS_SHOW_TYPE as c_ulong;
pub const ASN1_STRFLGS_DUMP_ALL: c_ulong = bindgen::ASN1_STRFLGS_DUMP_ALL as c_ulong;
pub const ASN1_STRFLGS_DUMP_UNKNOWN: c_ulong = bindgen::ASN1_STRFLGS_DUMP_UNKNOWN as c_ulong;
pub const ASN1_STRFLGS_DUMP_DER: c_ulong = bindgen::ASN1_STRFLGS_DUMP_DER as c_ulong;
pub const ASN1_STRFLGS_RFC2253: c_ulong = bindgen::ASN1_STRFLGS_RFC2253 as c_ulong;
pub const XN_FLAG_COMPAT: c_ulong = bindgen::XN_FLAG_COMPAT as c_ulong;
pub const XN_FLAG_SEP_MASK: c_ulong = bindgen::XN_FLAG_SEP_MASK as c_ulong;
pub const XN_FLAG_SEP_COMMA_PLUS: c_ulong = bindgen::XN_FLAG_SEP_COMMA_PLUS as c_ulong;
pub const XN_FLAG_SEP_CPLUS_SPC: c_ulong = bindgen::XN_FLAG_SEP_CPLUS_SPC as c_ulong;
pub const XN_FLAG_SEP_SPLUS_SPC: c_ulong = bindgen::XN_FLAG_SEP_SPLUS_SPC as c_ulong;
pub const XN_FLAG_SEP_MULTILINE: c_ulong = bindgen::XN_FLAG_SEP_MULTILINE as c_ulong;
pub const XN_FLAG_DN_REV: c_ulong = bindgen::XN_FLAG_DN_REV as c_ulong;
pub const XN_FLAG_FN_MASK: c_ulong = bindgen::XN_FLAG_FN_MASK as c_ulong;
pub const XN_FLAG_FN_SN: c_ulong = bindgen::XN_FLAG_FN_SN as c_ulong;
pub const XN_FLAG_SPC_EQ: c_ulong = bindgen::XN_FLAG_SPC_EQ as c_ulong;
pub const XN_FLAG_DUMP_UNKNOWN_FIELDS: c_ulong = bindgen::XN_FLAG_DUMP_UNKNOWN_FIELDS as c_ulong;
pub const XN_FLAG_RFC2253: c_ulong = bindgen::XN_FLAG_RFC2253 as c_ulong;
pub const XN_FLAG_ONELINE: c_ulong = bindgen::XN_FLAG_ONELINE as c_ulong;

// TODO(crbug.com/boringssl/596): Remove these wrappers.
#[cfg(unsupported_inline_wrappers)]
pub use { ERR_GET_LIB_RUST as ERR_GET_LIB,
          ERR_GET_REASON_RUST as ERR_GET_REASON,
          ERR_GET_FUNC_RUST as ERR_GET_FUNC,
          CBS_init_RUST as CBS_init,
          CBS_len_RUST as CBS_len };

pub fn init() {
    // This function does nothing.
    // TODO(davidben): Remove rust-openssl's dependency on this and remove this.
}
