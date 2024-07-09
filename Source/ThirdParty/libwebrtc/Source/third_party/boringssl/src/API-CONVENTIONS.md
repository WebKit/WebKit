# BoringSSL API Conventions

This document describes conventions for BoringSSL APIs. The [style
guide](./STYLE.md) also includes guidelines, but this document is targeted at
both API consumers and developers. API documentation in BoringSSL may assume
these conventions by default, rather than repeating them for every function.


## Documentation

All supported public APIs are documented in the public header files, found in
`include/openssl`. The API documentation is also available
[online](https://commondatastorage.googleapis.com/chromium-boringssl-docs/headers.html).

Experimental public APIs are found in `include/openssl/experimental`. Use of
these will likely be incompatible with changes in the near future as they are
finalized.

## Forward declarations

Do not write `typedef struct foo_st FOO` or try otherwise to define BoringSSL's
types. Including `openssl/base.h` (or `openssl/ossl_typ.h` for consumers who
wish to be OpenSSL-compatible) will forward-declare each type without importing
the rest of the library or invasive macros.


## Error-handling

Most functions in BoringSSL may fail, either due to allocation failures or input
errors. Functions which return an `int` typically return one on success and zero
on failure. Functions which return a pointer typically return `NULL` on failure.
However, due to legacy constraints, some functions are more complex. Consult the
API documentation before using a function.

On error, most functions also push errors on the error queue, an `errno`-like
mechanism. See the documentation for
[err.h](https://commondatastorage.googleapis.com/chromium-boringssl-docs/err.h.html)
for more details.

As with `errno`, callers must test the function's return value, not the error
queue to determine whether an operation failed. Some codepaths may not interact
with the error queue, and the error queue may have state from a previous failed
operation.

When ignoring a failed operation, it is recommended to call `ERR_clear_error` to
avoid the state interacting with future operations. Failing to do so should not
affect the actual behavior of any functions, but may result in errors from both
operations being mixed in error logging. We hope to
[improve](https://bugs.chromium.org/p/boringssl/issues/detail?id=38) this
situation in the future.

Where possible, avoid conditioning on specific reason codes and limit usage to
logging. The reason codes are very specific and may change over time.


## Memory allocation

BoringSSL allocates memory via `OPENSSL_malloc`, found in `mem.h`. Use
`OPENSSL_free`, found in the same header file, to release it. BoringSSL
functions will fail gracefully on allocation error, but it is recommended to use
a `malloc` implementation that `abort`s on failure.


## Pointers and slices

Unless otherwise specified, pointer parameters that refer to a single object,
either as an input or output parameter, may not be `NULL`. In this case,
BoringSSL often will not check for `NULL` before dereferencing, so passing
`NULL` may crash or exhibit other undefined behavior. (Sometimes the function
will check for `NULL` anyway, for OpenSSL compatibility, but we still consider
passing `NULL` to be a caller error.)

Pointer parameters may also refer to a contiguous sequence of objects, sometimes
referred to as a *slice*. These will typically be a pair of pointer and length
parameters named like `plaintext` and `plaintext_len`, or `objs` and `num_objs`.
We prefer the former for byte buffers and the latter for sequences of other
types. The documentation will usually refer to both parameters together, e.g.
"`EVP_DigestUpdate` hashes `len` bytes from `data`."

Parameters in C and C++ that use array syntax, such as
`uint8_t out[SHA256_DIGEST_LENGTH]`, are really pointers. In BoringSSL's uses of
this syntax, the pointer must point to the specified number of values.

In other cases, the documentation will describe how the function parameters
determine the slice's length. For example, a slice's length may be measured in
units other than element count, multiple slice parameters may share a length, or
a slice's length may be implicitly determined by other means like RSA key size.

By default, BoringSSL follows C++'s
[slice conventions](https://davidben.net/2024/01/15/empty-slices.html)
for pointers. That is, unless otherwise specified, pointers for non-empty
(non-zero length) slices must be represented by a valid pointer to that many
objects in memory. Pointers for empty (zero length) slices must either be `NULL`
or point within some sequence of objects of a compatible type.

WARNING: The dangling, non-null pointer used by Rust empty slices may *not* be
passed into BoringSSL. Rust FFIs must adjust such pointers to before passing to
BoringSSL. For example, see the `FfiSlice` abstraction in `bssl-crypto`. (We may
relax this if pointer arithmetic rules in C/C++ are adjusted to permit Rust's
pointers. Until then, it is impractical for a C/C++ library to act on such a
slice representation. See
[this document](https://davidben.net/2024/01/15/empty-slices.html) for more
discussion.)

In some cases, OpenSSL compatibility requires that a function will treat `NULL`
slice pointers differently from non-`NULL` pointers. Such behavior will be
described in documentation. For examples, see `EVP_EncryptUpdate`,
`EVP_DigestSignFinal`, and `HMAC_Init_ex`. Callers passing potentially empty
slices into such functions should take care that the `NULL` case is either
unreachable or still has the desired behavior.

If a `const char *` parameter is described as a "NUL-terminated string" or a
"C string", it must point to a sequence of `char` values containing a NUL (zero)
value, which determines the length. Unless otherwise specified, the pointer may
not be `NULL`, matching the C standard library.

For purposes of C and C++'s
[strict aliasing](https://en.cppreference.com/w/c/language/object#Strict_aliasing)
requirements, objects passed by pointers must be accessible as the specified
type. `uint8_t` may be assumed to be the same type as `unsigned char` and thus
may be the pointer type for all object types. BoringSSL does not support
platforms where `uint8_t` is a non-character type. However, there is no
strict aliasing sanitizer, very few C and C++ codebases are valid by strict
aliasing, and BoringSSL itself has some
[known strict aliasing bugs](https://crbug.com/boringssl/444), thus we strongly
recommend consumers build with `-fno-strict-aliasing`.

Pointer parameters additionally have ownership and lifetime requirements,
discussed in the section below.


## Object initialization and cleanup

BoringSSL defines a number of structs for use in its APIs. It is a C library,
so the caller is responsible for ensuring these structs are properly
initialized and released. Consult the documentation for a module for the
proper use of its types. Some general conventions are listed below.


### Heap-allocated types

Some types, such as `RSA`, are heap-allocated. All instances will be allocated
and returned from BoringSSL's APIs. It is an error to instantiate a heap-
allocated type on the stack or embedded within another object.

Heap-allocated types may have functioned named like `RSA_new` which allocates a
fresh blank `RSA`. Other functions may also return newly-allocated instances.
For example, `RSA_parse_public_key` is documented to return a newly-allocated
`RSA` object.

Heap-allocated objects must be released by the corresponding free function,
named like `RSA_free`. Like C's `free` and C++'s `delete`, all free functions
internally check for `NULL`. It is redundant to check for `NULL` before calling.

A heap-allocated type may be reference-counted. In this case, a function named
like `RSA_up_ref` will be available to take an additional reference count. The
free function must be called to decrement the reference count. It will only
release resources when the final reference is released. For OpenSSL
compatibility, these functions return `int`, but callers may assume they always
successfully return one because reference counts use saturating arithmetic.

C++ consumers are recommended to use `bssl::UniquePtr` to manage heap-allocated
objects. `bssl::UniquePtr<T>`, like other types, is forward-declared in
`openssl/base.h`. Code that needs access to the free functions, such as code
which destroys a `bssl::UniquePtr`, must include the corresponding module's
header. (This matches `std::unique_ptr`'s relationship with forward
declarations.) Note, despite the name, `bssl::UniquePtr` is also used with
reference-counted types. It owns a single reference to the object. To take an
additional reference, use the `bssl::UpRef` function, which will return a
separate `bssl::UniquePtr`.


### Stack-allocated types

Other types in BoringSSL are stack-allocated, such as `EVP_MD_CTX`. These
types may be allocated on the stack or embedded within another object.
However, they must still be initialized before use.

Every stack-allocated object in BoringSSL has a *zero state*, analogous to
initializing a pointer to `NULL`. In this state, the object may not be
completely initialized, but it is safe to call cleanup functions. Entering the
zero state cannot fail. (It is usually `memset(0)`.)

The function to enter the zero state is named like `EVP_MD_CTX_init` or
`CBB_zero` and will always return `void`. To release resources associated with
the type, call the cleanup function, named like `EVP_MD_CTX_cleanup`. The
cleanup function must be called on all codepaths, regardless of success or
failure. For example:

    uint8_t md[EVP_MAX_MD_SIZE];
    unsigned md_len;
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);  /* Enter the zero state. */
    int ok = EVP_DigestInit_ex(&ctx, EVP_sha256(), NULL) &&
             EVP_DigestUpdate(&ctx, "hello ", 6) &&
             EVP_DigestUpdate(&ctx, "world", 5) &&
             EVP_DigestFinal_ex(&ctx, md, &md_len);
    EVP_MD_CTX_cleanup(&ctx);  /* Release |ctx|. */

Note that `EVP_MD_CTX_cleanup` is called whether or not the `EVP_Digest*`
operations succeeded. More complex C functions may use the `goto err` pattern:

      int ret = 0;
      EVP_MD_CTX ctx;
      EVP_MD_CTX_init(&ctx);

      if (!some_other_operation()) {
        goto err;
      }

      uint8_t md[EVP_MAX_MD_SIZE];
      unsigned md_len;
      if (!EVP_DigestInit_ex(&ctx, EVP_sha256(), NULL) ||
          !EVP_DigestUpdate(&ctx, "hello ", 6) ||
          !EVP_DigestUpdate(&ctx, "world", 5) ||
          !EVP_DigestFinal_ex(&ctx, md, &md_len) {
        goto err;
      }

      ret = 1;

    err:
      EVP_MD_CTX_cleanup(&ctx);
      return ret;

Note that, because `ctx` is set to the zero state before any failures,
`EVP_MD_CTX_cleanup` is safe to call even if the first operation fails before
`EVP_DigestInit_ex`. However, it would be illegal to move the `EVP_MD_CTX_init`
below the `some_other_operation` call.

As a rule of thumb, enter the zero state of stack-allocated structs in the
same place they are declared.

C++ consumers are recommended to use the wrappers named like
`bssl::ScopedEVP_MD_CTX`, defined in the corresponding module's header. These
wrappers are automatically initialized to the zero state and are automatically
cleaned up.


### Data-only types

A few types, such as `SHA_CTX`, are data-only types and do not require cleanup.
These are usually for low-level cryptographic operations. These types may be
used freely without special cleanup conventions.


### Ownership and lifetime

When working with allocated objects, it is important to think about *ownership*
of each object, or what code is responsible for releasing it. This matches the
corresponding notion in higher-level languages like C++ and Rust.

Ownership applies to both uniquely-owned types and reference-counted types. For
the latter, ownership means the code is responsible for releasing one
reference. Note a *reference* in BoringSSL refers to an increment (and eventual
decrement) of an object's reference count, not `T&` in C++. Thus, to "take a
reference" means to increment the reference count and take ownership of
decrementing it.

As BoringSSL's APIs are primarily in C, ownership and lifetime obligations are
not rigorously annotated in the type signatures or checked at compile-time.
Instead, they are described in
[API documentation](https://commondatastorage.googleapis.com/chromium-boringssl-docs/headers.html).
This section describes some conventions.

Unless otherwise documented, functions do not take ownership of pointer
arguments. The pointer typically must remain valid for the duration of the
function call. The function may internally copy information from the argument or
take a reference, but the caller is free to release its copy or reference at any
point after the call completes.

A function may instead be documented to *take* or *transfer* ownership of a
pointer. The caller must own the object before the function call and, after
transfer, no longer owns it. As a corollary, the caller may no longer reference
the object without a separate guarantee on the lifetime. The function may even
release the object before returning. Callers that wish to independently retain a
transfered object must therefore take a reference or make a copy before
transferring. Callers should also take note of whether the function is
documented to transfer pointers unconditionally or only on success. Unlike C++
and Rust, functions in BoringSSL typically only transfer on success.

Likewise, output pointers may be owning or non-owning. Unless otherwise
documented, functions output non-owning pointers. The caller is not responsible
for releasing the output pointer, but it must not use the pointer beyond its
lifetime. The pointer may be released when the parent object is released or even
sooner on state change in the parent object.

If documented to output a *newly-allocated* object or a *reference* or *copy* of
one, the caller is responsible for releasing the object when it is done.

By convention, functions named `get0` return non-owning pointers. Functions
named `new` or `get1` return owning pointers. Functions named `set0` take
ownership of arguments. Functions named `set1` do not. They typically take a
reference or make a copy internally. These names originally referred to the
effect on a reference count, but the convention applies equally to
non-reference-counted types.

API documentation may also describe more complex obligations. For instance, an
object may borrow a pointer for longer than the duration of a single function
call, in which case the caller must ensure the lifetime extends accordingly.

Memory errors are one of the most common and dangerous bugs in C and C++, so
callers are encouraged to make use of tools such as
[AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html) and
higher-level languages.


## Thread safety

BoringSSL is internally aware of the platform threading library and calls into
it as needed. Consult the API documentation for the threading guarantees of
particular objects. In general, stateless reference-counted objects like `RSA`
or `EVP_PKEY` which represent keys may typically be used from multiple threads
simultaneously, provided no thread mutates the key.
