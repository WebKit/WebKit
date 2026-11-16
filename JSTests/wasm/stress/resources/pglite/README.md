# PGlite test resources

This directory contains a vendored build of [PGlite](https://github.com/electric-sql/pglite),
PostgreSQL 17.5 compiled to WebAssembly via Emscripten.

## Licenses

The vendored binaries combine code from three projects:

| Component                                         | License                          | File                          |
| ------------------------------------------------- | -------------------------------- | ----------------------------- |
| PGlite       | PostgreSQL License (chosen from Apache-2.0 OR PostgreSQL dual license) | `LICENSE.pglite` |
| PostgreSQL 17.5 | PostgreSQL License | `LICENSE.PostgreSQL`          |
| Emscripten 3.1.74 (libc + JS runtime glue in `pglite.js` / `pglite.wasm`) | MIT OR NCSA (dual) | `LICENSE.Emscripten`          |
