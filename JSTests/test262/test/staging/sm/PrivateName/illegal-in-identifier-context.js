// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/

// Private names can't appear in contexts where plain identifiers are expected.

// Private names as binding identifiers.
assertThrowsInstanceOf(() => eval(`var #a;`), SyntaxError);
assertThrowsInstanceOf(() => eval(`let #a;`), SyntaxError);
assertThrowsInstanceOf(() => eval(`const #a = 0;`), SyntaxError);
assertThrowsInstanceOf(() => eval(`function #a(){}`), SyntaxError);
assertThrowsInstanceOf(() => eval(`function f(#a){}`), SyntaxError);

// With escape sequences (leading and non-leading case).
assertThrowsInstanceOf(() => eval(String.raw`var #\u0061;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`var #a\u0061;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`let #\u0061;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`let #a\u0061;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`const #\u0061 = 0;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`const #a\u0061 = 0;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`function #\u0061(){}`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`function #a\u0061(){}`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`function f(#\u0061){}`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`function f(#a\u0061){}`), SyntaxError);


// Private names as label identifiers.
assertThrowsInstanceOf(() => eval(`#a: ;`), SyntaxError);

// With escape sequences (leading and non-leading case).
assertThrowsInstanceOf(() => eval(String.raw`#\u0061: ;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`#a\u0061: ;`), SyntaxError);


// Private names as identifier references.
assertThrowsInstanceOf(() => eval(`#a = 0;`), SyntaxError);
assertThrowsInstanceOf(() => eval(`typeof #a;`), SyntaxError);

// With escape sequences (leading and non-leading case).
assertThrowsInstanceOf(() => eval(String.raw`#\u0061 = 0;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`#a\u0061 = 0;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`typeof #\u0061;`), SyntaxError);
assertThrowsInstanceOf(() => eval(String.raw`typeof #a\u0061;`), SyntaxError);


