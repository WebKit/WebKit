bssl-crypto
============

Rust bindings to BoringSSL which wrap bssl-sys. Before using this crate, first [set up `bssl-sys`](../bssl-sys/README.md).

Then to run all tests:
```
cd rust/bssl-crypto && cargo clippy && cargo deny check && cargo test
```

Unlike BoringSSL itself, this crate does not attempt to handle allocation failures. If an allocation fails, functions in this crate will panic.

Like BoringSSL's API, the crate's API is not strictly stable. We may iterate on it as necessary to meet the needs of our consumers. See [this discussion](../../BREAKING-CHANGES.md) for general notes on how BoringSSL approaches breaking changes.

This crate must be updated atomically with BoringSSL, as it may depend on implementation details of the library. (For example, Rust has different expectations on struct movability than C/C++ APIs typically promise.)
