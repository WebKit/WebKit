// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
  description: >
      `yield` is a reserved identifier in strict mode code and may not be used
      as a label.
  es6id: 12.1.1
  negative:
    phase: parse
    type: SyntaxError
  flags: [onlyStrict]
 ---*/

throw "Test262: This statement should not be evaluated.";

yield: 1;
