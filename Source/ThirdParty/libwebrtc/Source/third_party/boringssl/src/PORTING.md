# Porting from OpenSSL to BoringSSL

BoringSSL is an OpenSSL derivative and is mostly source-compatible, for the
subset of OpenSSL retained. Libraries ideally need little to no changes for
BoringSSL support, provided they do not use removed APIs. In general, see if the
library compiles and, on failure, consult the documentation in the header files
and see if problematic features can be removed.

In some cases, BoringSSL-specific code may be necessary. In that case, the
`OPENSSL_IS_BORINGSSL` preprocessor macro may be used in `#ifdef`s. This macro
should also be used in lieu of the presence of any particular function to detect
OpenSSL vs BoringSSL in configure scripts, etc., where those are necessary.
Before using the preprocessor, however, contact the BoringSSL maintainers about
the missing APIs. If not an intentionally removed feature, BoringSSL will
typically add compatibility functions for convenience.

For convenience, BoringSSL defines upstream's `OPENSSL_NO_*` feature macros
corresponding to removed features. These may also be used to disable code which
uses a removed feature.

Note: BoringSSL does *not* have a stable API or ABI. It must be updated with its
consumers. It is not suitable for, say, a system library in a traditional Linux
distribution. For instance, Chromium statically links the specific revision of
BoringSSL it was built against. Likewise, Android's system-internal copy of
BoringSSL is not exposed by the NDK and must not be used by third-party
applications.


## Major API changes

### Integer types

Some APIs have been converted to use `size_t` for consistency and to avoid
integer overflows at the API boundary. (Existing logic uses a mismash of `int`,
`long`, and `unsigned`.)  For the most part, implicit casts mean that existing
code continues to compile. In some cases, this may require BoringSSL-specific
code, particularly to avoid compiler warnings.

Most notably, the `STACK_OF(T)` types have all been converted to use `size_t`
instead of `int` for indices and lengths.

### Reference counts

Some external consumers increment reference counts directly by calling
`CRYPTO_add` with the corresponding `CRYPTO_LOCK_*` value.

These APIs no longer exist in BoringSSL. Instead, code which increments
reference counts should call the corresponding `FOO_up_ref` function, such as
`EVP_PKEY_up_ref`. Note that not all of these APIs are present in OpenSSL and
may require `#ifdef`s.

### Error codes

OpenSSL's errors are extremely specific, leaking internals of the library,
including even a function code for the function which emitted the error! As some
logic in BoringSSL has been rewritten, code which conditions on the error may
break (grep for `ERR_GET_REASON` and `ERR_GET_FUNC`). This danger also exists
when upgrading OpenSSL versions.

Where possible, avoid conditioning on the exact error reason. Otherwise, a
BoringSSL `#ifdef` may be necessary. Exactly how best to resolve this issue is
still being determined. It's possible some new APIs will be added in the future.

Function codes have been completely removed. Remove code which conditions on
these as it will break with the slightest change in the library, OpenSSL or
BoringSSL.

### `*_ctrl` functions

Some OpenSSL APIs are implemented with `ioctl`-style functions such as
`SSL_ctrl` and `EVP_PKEY_CTX_ctrl`, combined with convenience macros, such as

    # define SSL_CTX_set_mode(ctx,op) \
            SSL_CTX_ctrl((ctx),SSL_CTRL_MODE,(op),NULL)

In BoringSSL, these macros have been replaced with proper functions. The
underlying `_ctrl` functions have been removed.

For convenience, `SSL_CTRL_*` values are retained as macros to `doesnt_exist` so
existing code which uses them (or the wrapper macros) in `#ifdef` expressions
will continue to function. However, the macros themselves will not work.

Switch any `*_ctrl` callers to the macro/function versions. This works in both
OpenSSL and BoringSSL. Note that BoringSSL's function versions will be
type-checked and may require more care with types. See the end of this
document for a table of functions to use.

### HMAC `EVP_PKEY`s

`EVP_PKEY_HMAC` is removed. Use the `HMAC_*` functions in `hmac.h` instead. This
is compatible with OpenSSL.

### DSA `EVP_PKEY`s

`EVP_PKEY_DSA` is deprecated. It is currently still possible to parse DER into a
DSA `EVP_PKEY`, but signing or verifying with those objects will not work.

### DES

The `DES_cblock` type has been switched from an array to a struct to avoid the
pitfalls around array types in C. Where features which require DES cannot be
disabled, BoringSSL-specific codepaths may be necessary.

### TLS renegotiation

OpenSSL enables TLS renegotiation by default and accepts renegotiation requests
from the peer transparently. Renegotiation is an extremely problematic protocol
feature, so BoringSSL rejects peer renegotiations by default.

To enable renegotiation, call `SSL_set_renegotiate_mode` and set it to
`ssl_renegotiate_once` or `ssl_renegotiate_freely`. Renegotiation is only
supported as a client in SSL3/TLS and the HelloRequest must be received at a
quiet point in the application protocol. This is sufficient to support the
common use of requesting a new client certificate between an HTTP request and
response in (unpipelined) HTTP/1.1.

Things which do not work:

* There is no support for renegotiation as a server.

* There is no support for renegotiation in DTLS.

* There is no support for initiating renegotiation; `SSL_renegotiate` always
  fails and `SSL_set_state` does nothing.

* Interleaving application data with the new handshake is forbidden.

* If a HelloRequest is received while `SSL_write` has unsent application data,
  the renegotiation is rejected.

### Lowercase hexadecimal

BoringSSL's `BN_bn2hex` function uses lowercase hexadecimal digits instead of
uppercase. Some code may require changes to avoid being sensitive to this
difference.

### Legacy ASN.1 functions

OpenSSL's ASN.1 stack uses `d2i` functions for parsing. They have the form:

    RSA *d2i_RSAPrivateKey(RSA **out, const uint8_t **inp, long len);

In addition to returning the result, OpenSSL places it in `*out` if `out` is
not `NULL`. On input, if `*out` is not `NULL`, OpenSSL will usually (but not
always) reuse that object rather than allocating a new one. In BoringSSL, these
functions are compatibility wrappers over a newer ASN.1 stack. Even if `*out`
is not `NULL`, these wrappers will always allocate a new object and free the
previous one.

Ensure that callers do not rely on this object reuse behavior. It is
recommended to avoid the `out` parameter completely and always pass in `NULL`.
Note that less error-prone APIs are available for BoringSSL-specific code (see
below).

## Optional BoringSSL-specific simplifications

BoringSSL makes some changes to OpenSSL which simplify the API but remain
compatible with OpenSSL consumers. In general, consult the BoringSSL
documentation for any functions in new BoringSSL-only code.

### Return values

Most OpenSSL APIs return 1 on success and either 0 or -1 on failure. BoringSSL
has narrowed most of these to 1 on success and 0 on failure. BoringSSL-specific
code may take advantage of the less error-prone APIs and use `!` to check for
errors.

### Initialization

OpenSSL has a number of different initialization functions for setting up error
strings and loading algorithms, etc. All of these functions still exist in
BoringSSL for convenience, but they do nothing and are not necessary.

The one exception is `CRYPTO_library_init`. In `BORINGSSL_NO_STATIC_INITIALIZER`
builds, it must be called to query CPU capabitilies before the rest of the
library. In the default configuration, this is done with a static initializer
and is also unnecessary.

### Threading

OpenSSL provides a number of APIs to configure threading callbacks and set up
locks. Without initializing these, the library is not thread-safe. Configuring
these does nothing in BoringSSL. Instead, BoringSSL calls pthreads and the
corresponding Windows APIs internally and is always thread-safe where the API
guarantees it.

### ASN.1

BoringSSL is in the process of deprecating OpenSSL's `d2i` and `i2d` in favor of
new functions using the much less error-prone `CBS` and `CBB` types.
BoringSSL-only code should use those functions where available.


## Replacements for `CTRL` values

When porting code which uses `SSL_CTX_ctrl` or `SSL_ctrl`, use the replacement
functions below. If a function has both `SSL_CTX` and `SSL` variants, only the
`SSL_CTX` version is listed.

Note some values correspond to multiple functions depending on the `larg`
parameter.

`CTRL` value | Replacement function(s)
-------------|-------------------------
`DTLS_CTRL_GET_TIMEOUT` | `DTLSv1_get_timeout`
`DTLS_CTRL_HANDLE_TIMEOUT` | `DTLSv1_handle_timeout`
`SSL_CTRL_CHAIN` | `SSL_CTX_set0_chain` or `SSL_CTX_set1_chain`
`SSL_CTRL_CHAIN_CERT` | `SSL_add0_chain_cert` or `SSL_add1_chain_cert`
`SSL_CTRL_CLEAR_EXTRA_CHAIN_CERTS` | `SSL_CTX_clear_extra_chain_certs`
`SSL_CTRL_CLEAR_MODE` | `SSL_CTX_clear_mode`
`SSL_CTRL_CLEAR_OPTIONS` | `SSL_CTX_clear_options`
`SSL_CTRL_EXTRA_CHAIN_CERT` | `SSL_CTX_add_extra_chain_cert`
`SSL_CTRL_GET_CHAIN_CERTS` | `SSL_CTX_get0_chain_certs`
`SSL_CTRL_GET_CLIENT_CERT_TYPES` | `SSL_get0_certificate_types`
`SSL_CTRL_GET_EXTRA_CHAIN_CERTS` | `SSL_CTX_get_extra_chain_certs` or `SSL_CTX_get_extra_chain_certs_only`
`SSL_CTRL_GET_MAX_CERT_LIST` | `SSL_CTX_get_max_cert_list`
`SSL_CTRL_GET_NUM_RENEGOTIATIONS` | `SSL_num_renegotiations`
`SSL_CTRL_GET_READ_AHEAD` | `SSL_CTX_get_read_ahead`
`SSL_CTRL_GET_RI_SUPPORT` | `SSL_get_secure_renegotiation_support`
`SSL_CTRL_GET_SESSION_REUSED` | `SSL_session_reused`
`SSL_CTRL_GET_SESS_CACHE_MODE` | `SSL_CTX_get_session_cache_mode`
`SSL_CTRL_GET_SESS_CACHE_SIZE` | `SSL_CTX_sess_get_cache_size`
`SSL_CTRL_GET_TLSEXT_TICKET_KEYS` | `SSL_CTX_get_tlsext_ticket_keys`
`SSL_CTRL_GET_TOTAL_RENEGOTIATIONS` | `SSL_total_renegotiations`
`SSL_CTRL_MODE` | `SSL_CTX_get_mode` or `SSL_CTX_set_mode`
`SSL_CTRL_NEED_TMP_RSA` | `SSL_CTX_need_tmp_RSA` is equivalent, but [*do not use this function*](https://freakattack.com/). (It is a no-op in BoringSSL.)
`SSL_CTRL_OPTIONS` | `SSL_CTX_get_options` or `SSL_CTX_set_options`
`SSL_CTRL_SESS_NUMBER` | `SSL_CTX_sess_number`
`SSL_CTRL_SET_CURVES` | `SSL_CTX_set1_curves`
`SSL_CTRL_SET_ECDH_AUTO` | `SSL_CTX_set_ecdh_auto`
`SSL_CTRL_SET_MAX_CERT_LIST` | `SSL_CTX_set_max_cert_list`
`SSL_CTRL_SET_MAX_SEND_FRAGMENT` | `SSL_CTX_set_max_send_fragment`
`SSL_CTRL_SET_MSG_CALLBACK` | `SSL_set_msg_callback`
`SSL_CTRL_SET_MSG_CALLBACK_ARG` | `SSL_set_msg_callback_arg`
`SSL_CTRL_SET_MTU` | `SSL_set_mtu`
`SSL_CTRL_SET_READ_AHEAD` | `SSL_CTX_set_read_ahead`
`SSL_CTRL_SET_SESS_CACHE_MODE` | `SSL_CTX_set_session_cache_mode`
`SSL_CTRL_SET_SESS_CACHE_SIZE` | `SSL_CTX_sess_set_cache_size`
`SSL_CTRL_SET_TLSEXT_HOSTNAME` | `SSL_set_tlsext_host_name`
`SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG` | `SSL_CTX_set_tlsext_servername_arg`
`SSL_CTRL_SET_TLSEXT_SERVERNAME_CB` | `SSL_CTX_set_tlsext_servername_callback`
`SSL_CTRL_SET_TLSEXT_TICKET_KEYS` | `SSL_CTX_set_tlsext_ticket_keys`
`SSL_CTRL_SET_TLSEXT_TICKET_KEY_CB` | `SSL_CTX_set_tlsext_ticket_key_cb`
`SSL_CTRL_SET_TMP_DH` | `SSL_CTX_set_tmp_dh`
`SSL_CTRL_SET_TMP_DH_CB` | `SSL_CTX_set_tmp_dh_callback`
`SSL_CTRL_SET_TMP_ECDH` | `SSL_CTX_set_tmp_ecdh`
`SSL_CTRL_SET_TMP_ECDH_CB` | `SSL_CTX_set_tmp_ecdh_callback`
`SSL_CTRL_SET_TMP_RSA` | `SSL_CTX_set_tmp_rsa` is equivalent, but [*do not use this function*](https://freakattack.com/). (It is a no-op in BoringSSL.)
`SSL_CTRL_SET_TMP_RSA_CB` | `SSL_CTX_set_tmp_rsa_callback` is equivalent, but [*do not use this function*](https://freakattack.com/). (It is a no-op in BoringSSL.)
