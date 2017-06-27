## Integrity Test

FIPS-140 mandates that a module calculate an HMAC of its own code in a constructor function and compare the result to a known-good value. Typical code produced by a C compiler includes large numbers of relocations: places in the machine code where the linker needs to resolve and inject the final value of a symbolic expression. These relocations mean that the bytes that make up any specific bit of code generally aren't known until the final link has completed.

Additionally, because of shared libraries and ASLR, some relocations can only be resolved at run-time, and thus targets of those relocations vary even after the final link.

BoringSSL is linked (often statically) into a large number of binaries. It would be a significant cost if each of these binaries had to be post-processed in order to calculate the known-good HMAC value. We would much prefer if the value could be calculated, once, when BoringSSL itself is compiled.

In order for the value to be calculated before the final link, there can be no relocations in the hashed code and data. This document describes how we build C and assembly code in order to produce an object file containing all the code and data for the FIPS module without that code having any relocations.

First, all the C source files for the module are compiled as a single unit by compiling a single source file that `#include`s them all (this is `bcm.c`). The `-fPIC` flag is used to cause the compiler to use IP-relative addressing in many (but not all) cases. Also the `-S` flag is used to instruct the compiler to produce a textual assembly file rather than a binary object file.

The textual assembly file is then processed by a script to merge in assembly implementations of some primitives and to eliminate the remaining sources of relocations.

##### Redirector functions

The most obvious cause of relocations are out-calls from the module to non-cryptographic functions outside of the module. Most obviously these include `malloc`, `memcpy` and other libc functions, but also include calls to support code in BoringSSL such as functions for managing the error queue.

Offsets to these functions cannot be known until the final link because only the linker sees the object files containing them. Thus calls to these functions are rewritten into an IP-relative jump to a redirector function. The redirector functions contain a single jump instruction to the real function and are placed outside of the module and are thus not hashed (see diagram).

![module structure](/crypto/fipsmodule/intcheck1.png)

In this diagram, the integrity check hashes from `module_start` to `module_end`. Since this does not cover the jump to `memcpy`, it's fine that the linker will poke the final offset into that instruction.

##### Read-only data

Normally read-only data is placed in a `.data` segment that doesn't get mapped into memory with execute permissions. However, the offset of the data segment from the text segment is another thing that isn't determined until the final link. In order to fix data offsets before the link, read-only data is simply placed in the module's `.text` segment. This might make building ROP chains easier for an attacker, but so it goes.

One special case is `rel.ro` data, which is data that contains function pointers. Since these function pointers are absolute, they are written by the dynamic linker at run-time and so we must eliminate them. The pattern that causes them is when we have a static `EVP_MD` or `EVP_CIPHER` object thus, inside the module, we'll change this pattern to instead to reserve space in the BSS for the object, and add a `CRYPTO_once_t` to protect its initialisation. The script will generate functions outside of the module that return pointers to these areas of memory—they effectively act like a special-purpose malloc calls that cannot fail.

##### Read-write data

Mutable data is a problem. It cannot be in the text segment because the text segment is mapped read-only. If it's in a different segment then the code cannot reference it with a known, IP-relative offset because the segment layout is only fixed during the final link.

Thankfully, mutable data is very rare in our cryptographic code and I hope that we can get it down to just a few variables. In order to allow this we use a similar design to the redirector functions: the code references a symbol that's in the text segment, but out of the module and thus not hashed. A relocation record is emitted to instruct the linker to poke the final offset to the variable in that location. Thus the only change needed is an extra indirection when loading the value.

##### Other transforms

The script performs a number of other transformations which are worth noting but do not warrant their own sections:

1.  It duplicates each global symbol with a local symbol that has `_local_target` appended to the name. References to the global symbols are rewritten to use these duplicates instead. Otherwise, although the generated code uses IP-relative references, relocations are emitted for global symbols in case they are overridden by a different object file during the link.
1.  Various sections, notably `.rodata`, are moved to the `.text` section, inside the module, so module code may reference it without relocations.
1.  For each BSS symbol, it generates a function named after that symbol but with `_bss_get` appended, which returns its address.
1.  It inserts the labels that delimit the module's code and data (called `module_start` and `module_end` in the diagram above).
1.  It adds a 32-byte, read-only array outside of the module to contain the known-good HMAC value.
1.  It rewrites some "dummy" references to point to those labels and that array. In order to get the C compiler to emit the correct code it's necessary to make it think that it's referencing static functions. This compensates for that trick.

##### Integrity testing

In order to actually implement the integrity test, a constructor function within the module calculates an HMAC from `module_start` to `module_end` using a fixed, all-zero key. It compares the result with the known-good value added (by the script) to the unhashed portion of the text segment. If they don't match, it calls `exit` in an infinite loop.

Initially the known-good value will be incorrect. Another script (`inject-hash.go`) calculates the correct value from the assembled object and injects it back into the object.

![build process](/crypto/fipsmodule/intcheck2.png)

### Comparison with OpenSSL's method

(This is based on reading OpenSSL's [user guide](https://www.openssl.org/docs/fips/UserGuide-2.0.pdf) and inspecting the code of OpenSSL FIPS 2.0.12.)

OpenSSL's solution to this problem is broadly similar but has a number of differences:

1.  OpenSSL deals with run-time relocations by not hashing parts of the module's data. BoringSSL will eliminate run-time relocations instead and hash everything.
1.  OpenSSL uses `ld -r` (the partial linking mode) to merge a number of object files into their `fipscanister.o`. For BoringSSL, we propose to merge all the C source files by building a single C file that #includes all the others, then we propose to merge the assembly sources by concatenating them to the assembly output from the C compiler.
1.  OpenSSL depends on the link order and inserts two object files, `fips_start.o` and `fips_end.o`, in order to establish the `module_start` and `module_end` values. BoringSSL proposes to simply add labels at the correct places in the assembly.
1.  OpenSSL calculates the hash after the final link and either injects it into the binary or recompiles with the value of the hash passed in as a #define. BoringSSL calculates it prior to the final link and injects it into the object file.
1.  OpenSSL references read-write data directly, since it can know the offsets to it. BoringSSL indirects these loads and stores.
1.  OpenSSL doesn't run the power-on test until `FIPS_module_mode_set` is called, BoringSSL plans to do it in a constructor function. Failure of the test is non-fatal in OpenSSL, BoringSSL plans to crash.
1.  Since the contents of OpenSSL's module change between compilation and use, OpenSSL generates `fipscanister.o.sha1` to check that the compiled object doesn't change before linking. Since BoringSSL's module is fixed after compilation, the final integrity check is unbroken through the linking process.

Some of the similarities are worth noting:

1.  OpenSSL has all out-calls from the module indirecting via the PLT, which is equivalent to the redirector functions described above.


![OpenSSL build process](/crypto/fipsmodule/intcheck3.png)
