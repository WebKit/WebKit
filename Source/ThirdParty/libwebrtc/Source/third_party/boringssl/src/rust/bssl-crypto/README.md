bssl-crypto
============

Rust bindings to BoringSSL which wrap bssl-sys. Before using this crate, first [set up `bssl-sys`](../bssl-sys/README.md).

Then to run all tests:
```
cd rust/bssl-crypto && cargo clippy && cargo deny check && cargo test
```
