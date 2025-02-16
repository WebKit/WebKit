/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var a = [];

// Test up to 200 to cover tunables such as js::PropertyTree::MAX_HEIGHT.
for (var i = 0; i < 200; i++) {
    a.push("b" + i);
    assertThrowsInstanceOfWithMessage(
        () => eval("(function ([" + a.join("],[") + "],a,a){})"),
        SyntaxError,
        'duplicate argument names not allowed in this context');
}
