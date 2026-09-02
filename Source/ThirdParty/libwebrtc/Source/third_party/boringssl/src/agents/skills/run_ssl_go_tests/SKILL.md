---
name: run-ssl-go-tests
description: How to run BoringSSL's ssl/test/runner suite (protocol-level BoGo tests)
---

# Running BoringSSL Protocol-Level Tests (BoGo Tests aka blackbox tests)

This skill describes how to run and configure BoringSSL's protocol-level test suite, which uses a Go-based test harness (`runner`) and a C++ shim (`bssl_shim`).

To run and work with these tests, you must refer to the following documentation:

1.  **Overview and Running**: Read [ssl/test/README.md](../../../ssl/test/README.md) for an overview of the test suite and basic instructions on how to run it manually using `go test` in the `ssl/test/runner` directory.
2.  **Build Integration**: Refer to [BUILDING.md](../../../BUILDING.md) (specifically the "Running Tests" section) for how these tests are integrated into the CMake/Ninja build system (e.g., `ninja run_tests`).

## Quick Reference

*   **Running all tests via Go**:
    ```bash
    cd ssl/test/runner && go test
    ```
*   **Listing/Filtering flags**:
    To see available flags for the runner, build the test binary and run with `-help`:
    ```bash
    cd ssl/test/runner
    go test -c
    ./runner.test -help
    ```
