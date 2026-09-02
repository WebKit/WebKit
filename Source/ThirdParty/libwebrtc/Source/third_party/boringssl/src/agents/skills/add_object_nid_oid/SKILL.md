---
name: add-object-nid-oid
description: How to add a new object with a NID and/or OID to BoringSSL.
---

# Adding a New NID or OID

To add a new ASN.1 object (with a NID and/or OID) to BoringSSL, you must add it to the objects definition file and regenerate the generated headers and lookup tables.

Refer to [crypto/obj/README.md](../../../crypto/obj/README.md) for an overview of the generated files.

## Steps

1.  **Update `objects.txt`**:
    *   Add the new object to [crypto/obj/objects.txt](../../../crypto/obj/objects.txt).
    *   Follow the format described in [crypto/obj/README.md](../../../crypto/obj/README.md) (in the section titled "`objects.txt` Format Reference").

2.  **Regenerate Files**:
    *   The generator script [crypto/obj/objects.go](../../../crypto/obj/objects.go) **must be run from the `crypto/obj` directory** because it relies on relative paths for its inputs and outputs.
    *   Run the following command:
        ```bash
        cd crypto/obj && go run objects.go
        ```
    *   This will update `obj_mac.num` (to stabilize NIDs), `obj_dat.h`, and `include/openssl/nid.h`.
