# Fiat Cryptography

Most files in this directory are generated using [Fiat
Cryptography](https://github.com/mit-plv/fiat-crypto) from the associated
library of arithmetic-implementation templates. These files are included under
the Apache 2.0 license. (See LICENSE file.)

Some files are included directly from the `fiat-c/src` directory of the Fiat
Cryptography repository. Their contents are `#include`d into source files, so
we rename them to `.h`. Implementations that use saturated arithmetic on 64-bit
words are further manually edited to use platform-appropriate incantations for
operations such as addition with carry; these changes are marked with "`NOTE:
edited after generation`".

# CryptOpt

Files in the `asm` directory are compiled from Fiat-Cryptography templates
using [CryptOpt](https://github.com/0xADE1A1DE/CryptOpt). These generated
assembly files have been edited to support call-stack unwinding. The modified
files have been checked for functional correctness using the CryptOpt
translation validator that is included in the Fiat-Cryptography repository.
Correct unwinding and manual assembler-directive changes related to object-file
conventions are validated using unit tests.

# Bedrock

Routines for which assembly-level optimization is not necessary are written in
[Bedrock2](https://github.com/mit-plv/bedrock2) and translated to C. The file
`bedrock_unverified_platform.c.inc` provides the platform functionality code
thus translated expects.

The P256 point doubling and addition are implemented in this manner in
p256_coord.br.c.inc. This code originates from Fiat Cryptogaphy commit
6e22d333861d51c7f6baa0826648a2215d0c333f file src/Bedrock/P256.v
Field arithmetic operations are dispatched in p256_field.c.inc to
platform-specific code including assembly, fiat-crypto-generated C, and
Bedrock-generated C.
