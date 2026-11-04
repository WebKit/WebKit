---
name: add-error-code
description: How to add a new error code in BoringSSL.
---

# Adding a New Error Code

To add a new error code, you only need to use it in the source code and then run the error generation script. The script will automatically define it in the header and update the error database.

## Steps

1.  **Use the Error Code**:
    *   Use the new error code directly in your C/C++ source file (e.g., `EVP_R_NEW_ERROR_CODE`). It must start with the correct library prefix (e.g., `EVP_R_`, `SSL_R_`).

2.  **Run the Error Generator**:
    *   Run [util/make_errors.go](../../../util/make_errors.go), passing the lowercase library name (e.g., `evp`, `ssl`).
    *   Example:
        ```bash
        go run util/make_errors.go evp
        ```
    *   The script will automatically:
        *   Find the new error code in the source.
        *   Assign it a numeric value.
        *   Add the `#define` to the appropriate header in [include/openssl](../../../include/openssl).
        *   Update the `.errordata` file in [crypto/err](../../../crypto/err).
