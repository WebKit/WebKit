# BoringSSL Style Guide

BoringSSL usually follows the
[Google C++ style guide](https://google.github.io/styleguide/cppguide.html),
The rest of this document describes differences and clarifications on
top of the base guide.


## Legacy code

As a derivative of OpenSSL, BoringSSL contains a lot of legacy code that
does not follow this style guide. Particularly where public API is
concerned, balance consistency within a module with the benefits of a
given rule. Module-wide deviations on naming should be respected while
integer and return value conventions take precedence over consistency.


## Language

Although BoringSSL is, now, written in C++, it has a long history from C, so
some parts of the Google C++ style guide cannot be applied directly. In
particular, our public API overlaps highly with OpenSSL, and our public headers
must be C-compatible.

The implementation also used to be in C and has been incrementally adapted to
C++, so much of our code reflects our C conventions rather than Google C++. If
unsure, match surrounding code. Discrepancies between it and Google C++ style
will be fixed over time.

In C code, BoringSSL does not limit itself to old versions of C. (See
[BUILDING.md](./BUILDING.md) for current requirements.) In particular:

* Variable declarations in the middle of a function or inside a `for` loop
  are allowed and preferred where possible. Note that the common `goto err`
  cleanup pattern requires lifting some variable declarations.

* Comments should be `// C99-style` for consistency with C++.

* For new constants, prefer enums when the values are sequential. If adding
  values to an existing set of `#define`s, continue with `#define`.

In both C and C++, when declaring pointer or reference types, `*` or `&` should
be placed next to the variable name, not the type. So

    uint8_t *ptr;
    const auto &blah;

not

    uint8_t* ptr;
    const auto& blah;


### Memory allocation

When allocating memory in libcrypto and libssl, do not use `malloc()` and
`free()` from C, or `new` and `delete` from C++. Instead, use the wrappers
`OPENSSL_malloc()` and `OPENSSL_free()` from `<openssl/mem.h>`, or the internal
C++ helpers `bssl::New`, `bssl::Delete`, and `bssl::MakeUnique` from
`crypto/mem_internal.h`.

Memory allocation from these functions is fallible. Calling code should check
for allocation failure and handle it gracefully. `OPENSSL_free` additionally
overwrites memory before freeing it.

We may relax these requirements in the future to align with standard C++, after
evaluating where we need fallible allocations and other ways to handle
overwriting memory.

### C standard library functions

With the exception of memory allocation above, code in BoringSSL can freely use
C standard library functions. In particular, use the standard C `assert()` as
appropriate. (Note that `assert()` is a debug-only assert. Use `BSSL_CHECK()`
for a check that should be run in release builds.)

Some standard library functions are impacted by a
[language bug](https://davidben.net/2024/01/15/empty-slices.html) around
zero-length inputs. Although this
[will be fixed](https://developers.redhat.com/articles/2024/12/11/making-memcpynull-null-0-well-defined)
in C29, we cannot rely on this. Instead, use the following wrappers, found in
`crypto/internal.h`:

* `OPENSSL_memchr`
* `OPENSSL_memcmp`
* `OPENSSL_memcpy`
* `OPENSSL_memmove`
* `OPENSSL_memset`

### C++ standard library functions

With the exception of memory allocation above, code in BoringSSL can freely use
C++ standard library functions. Note this excludes most container types, which
may allocate. Instead, `crypto/mem_internal.h` contains some replacement
containers that use `OPENSSL_malloc`. Like `OPENSSL_malloc`, memory allocation
is fallible in this containers.

We may relax these requirements in the future to align with standard C++, after
evaluating where we need fallible allocations and other ways to handle
overwriting memory.


## Formatting

Single-statement blocks are not allowed. All conditions and loops must
use braces:

    if (foo) {
      do_something();
    }

not

    if (foo)
      do_something();


## Integers

Prefer using explicitly-sized integers where appropriate rather than
generic C ones. For instance, to represent a byte, use `uint8_t`, not
`unsigned char`. Likewise, represent a two-byte field as `uint16_t`, not
`unsigned short`.

Sizes are represented as `size_t`.

Within a struct that is retained across the lifetime of an SSL
connection, if bounds of a size are known and it's easy, use a smaller
integer type like `uint8_t`. This is a "free" connection footprint
optimization for servers. Don't make code significantly more complex for
it, and do still check the bounds when passing in and out of the
struct. This narrowing should not propagate to local variables and
function parameters.

When doing arithmetic, account for overflow conditions.

Except with platform APIs, do not use `ssize_t`. MSVC lacks it, and
prefer out-of-band error signaling for `size_t` (see Return values).


## Naming

Follow Google naming conventions for C++.

For C symbols, use the following naming conventions for consistency with
existing OpenSSL and C styles:

* Define structs with typedef named `TYPE_NAME`. The corresponding struct
  should be named `struct type_name_st`.

* Name public functions as `MODULE_function_name`, unless the module
  already uses a different naming scheme for legacy reasons. The module
  name should be a type name if the function is a method of a particular
  type.

* Some types are allocated within the library while others are initialized
  into a struct allocated by the caller, often on the stack. Name these
  functions `TYPE_NAME_new`/`TYPE_NAME_free` and
  `TYPE_NAME_init`/`TYPE_NAME_cleanup`, respectively. All `TYPE_NAME_free`
  functions must do nothing on `NULL` input.

* Name enums like `enum unix_hacker_t`. For instance:

      enum should_free_handshake_buffer_t {
        free_handshake_buffer,
        dont_free_handshake_buffer,
      };


In both languages, if a variable is the number of objects at some pointer, it
has the suffix `_len` when the elements are each 1-byte and the prefix `num_`
otherwise. An output parameter is named `out` or has an `out_` prefix. For
instance:

    uint8_t *out,
    size_t *out_len,
    const uint8_t *in,
    size_t in_len,
    const uint32_t *codepoints,
    size_t num_codepoints,

Where possible, prefer to use `bssl::Span` from `<openssl/span.h>`, instead of
separating pointer and length parameters. This is, however, not possible in
public functions, which must remain C-compatible.

Name public headers like `include/openssl/evp.h` with header guards like
`OPENSSL_HEADER_EVP_H`. Name internal headers like
`crypto/ec/internal.h` with header guards like
`OPENSSL_HEADER_CRYPTO_EC_INTERNAL_H`.


## Return values

As even `malloc` may fail in BoringSSL, the vast majority of functions
will have a failure case. Functions should return `int` with one on
success and zero on error. Do not overload the return value to both
signal success/failure and output an integer. For example:

    OPENSSL_EXPORT int CBS_get_u16(CBS *cbs, uint16_t *out);

If a function needs more than a true/false result code, define an enum
rather than arbitrarily assigning meaning to int values.

If a function outputs a pointer to an object on success and there are no
other outputs, return the pointer directly and `NULL` on error.


## Parameters

Where not constrained by legacy code, parameter order should be:

1. context parameters
2. output parameters
3. input parameters

For example,

    /* CBB_add_asn sets `*out_contents` to a `CBB` into which the contents of an
     * ASN.1 object can be written. The `tag` argument will be used as the tag for
     * the object. It returns one on success or zero on error. */
    OPENSSL_EXPORT int CBB_add_asn1(CBB *cbb, CBB *out_contents, unsigned tag);


## Documentation

All public symbols must have a documentation comment in their header
file. The style is based on that of Go. The first sentence begins with
the symbol name, optionally prefixed with "A" or "An". Apart from the
initial mention of symbol, references to other symbols or parameter
names should be surrounded by `` `backticks` ``.

Documentation should be concise but completely describe the exposed
behavior of the function. Pay special note to success/failure behaviors
and caller obligations on object lifetimes. If this sacrifices
conciseness, consider simplifying the function's behavior.

    // EVP_DigestVerifyUpdate appends `len` bytes from `data` to the data which
    // will be verified by `EVP_DigestVerifyFinal`. It returns one on success and
    // zero otherwise.
    OPENSSL_EXPORT int EVP_DigestVerifyUpdate(EVP_MD_CTX *ctx, const void *data,
                                              size_t len);

Explicitly mention any surprising edge cases or deviations from common
return value patterns in legacy functions.

    // RSA_private_encrypt encrypts `flen` bytes from `from` with the private key in
    // `rsa` and writes the encrypted data to `to`. The `to` buffer must have at
    // least `RSA_size` bytes of space. It returns the number of bytes written, or
    // -1 on error. The `padding` argument must be one of the `RSA_*_PADDING`
    // values. If in doubt, `RSA_PKCS1_PADDING` is the most common.
    //
    // WARNING: this function is dangerous because it breaks the usual return value
    // convention. Use `RSA_sign_raw` instead.
    OPENSSL_EXPORT int RSA_private_encrypt(int flen, const uint8_t *from,
                                           uint8_t *to, RSA *rsa, int padding);

Document private functions in their `internal.h` header or, if static,
where defined.


## Build logic

BoringSSL is used by many projects with many different build tools.
Reimplementing and maintaining build logic in each downstream build is
cumbersome, so build logic should be avoided where possible. Platform-specific
files should be excluded by wrapping the contents in `#ifdef`s, rather than
computing platform-specific file lists. Generated source files such as perlasm
and `err_data.c` may be used in the standalone CMake build but, for downstream
builds, they should be pre-generated in `generate_build_files.py`.
