// Copyright (C) 2020 Sony Interactive Entertainment Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-literals-string-literals
description: >
    LegacyOctalEscapeSequence is not enabled in strict mode code
    (regardless of the presence of Annex B)
info: |
    A conforming implementation, when processing strict mode code, must not extend the
    syntax of EscapeSequence to include LegacyOctalEscapeSequence as described in B.1.2.
flags: [onlyStrict]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

'\9';
