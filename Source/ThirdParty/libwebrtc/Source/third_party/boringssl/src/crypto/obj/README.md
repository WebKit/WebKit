# Object identifier (NID and OID) generator

This directory provides NIDs (numbered values for ASN.1 object identifiers and
other symbols, used by other libraries to identify cryptographic primitives),
and code for generating NIDs and object identifiers (OIDs).

The files `nid.h`, `obj_mac.num`, and `obj_dat.h` are generated from
`objects.txt` and `obj_mac.num`. To regenerate them, run:

```
go run objects.go
```

The above command must be run from this directory, `crypto/obj`.

## Directory contents
* [`objects.txt`](./objects.txt) contains the list of all built-in OIDs. It is
  processed by [`objects.go`](./objects.go) to output `obj_mac.num`,
  `obj_dat.h`, and `nid.h`. See below for the format used in `objects.txt`.

* `obj_mac.num` is the list of NID values for each OID. This is an input/output
  file so NID values are stable across regenerations.

* `nid.h` is the header which defines macros for all the built-in OIDs in C.

* `obj_dat.h` contains the ASN1_OBJECTs corresponding to built-in OIDs
  themselves along with lookup tables for search by short name, OID, etc.

## `objects.txt` Format Reference

`objects.txt` is a sequence of object definitions and directives, each on one
line.

### Directives

Directives begin with `!`. Supported directives are:

*   `!module <name>`: Prefixes subsequent object names with `<name>_`.

*   `!global`: Resets the module prefix.

*   `!Cname <name>`: Overrides the name used in C symbols (e.g. `NID_<name>`)
    for the next object.

*   `!Alias <alias_name> <parent_path> <sibling_numbers...>`: Defines a reusable
    OID alias without emitting an object.

### Object Definitions

An object definition has the format `<OID_PATH> : <short_name> : <long_name>`:

*   `<OID_PATH>` defines the OID with space-separated integers (e.g. `1 2 840`),
    optionally beginning with a reference to a previously defined parent OID arc
    (e.g. `X500algorithms 1 1`).

*   `<short_name>` and `<long_name>` are the short and long names for the OID,
    respectively. At least one must be provided. If one is missing, it takes the
    value of the other, but the corresponding `LN_<name>` or `SN_<name>` symbol
    will not be defined.

Each object definition generates an entry in the objects table, and C symbols
like `NID_<name>`. By default, the C symbols are named after `long_name` (if it
has no spaces) or `short_name`. Spaces, dashes, and dots are replaced by
underscores. This can be overridden by `!Cname`.

For more examples, see [crypto/obj/objects.txt](./objects.txt).
