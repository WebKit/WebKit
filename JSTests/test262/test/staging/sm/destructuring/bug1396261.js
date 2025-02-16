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
// Ensure array or object literals with trailing property accessors are not
// treated as nested destructuring patterns in assignment destructuring
// contexts.

// Array destructuring with normal element.
[{a: 0}.x] = [];
[[0].x] = [];

// Array destructuring with spread element.
[...{a: 0}.x] = [];
[...[0].x] = [];

// Object destructuring with normal property.
({a: {b: 0}.x} = {});
({a: [0].x} = {});

// Object destructuring with spread property.
({...{b: 0}.x} = {});
({...[0].x} = {});

// Object literal with initializer shorthand in destructuring context.
assertThrowsInstanceOf(() => Function(`[{a = 0}.x] = [];`), SyntaxError);
assertThrowsInstanceOf(() => Function(`[...{a = 0}.x] = [];`), SyntaxError);
assertThrowsInstanceOf(() => Function(`({a: {b = 0}.x} = {});`), SyntaxError);
assertThrowsInstanceOf(() => Function(`({...{b = 0}.x} = {});`), SyntaxError);

// Object destructuring with "eval" or "arguments" shorthand in strict mode.
(function() {
    "use strict";

    // Ensure "eval" resp. "arguments" is not treated as an assignment.
    [{eval}.x] = [];
    [...{eval}.x] = [];
    ({a: {eval}.x} = {});
    ({...{eval}.x} = {});

    [{arguments}.x] = [];
    [...{arguments}.x] = [];
    ({a: {arguments}.x} = {});
    ({...{arguments}.x} = {});
})();

