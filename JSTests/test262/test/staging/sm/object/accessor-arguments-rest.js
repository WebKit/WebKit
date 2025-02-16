// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
assertThrowsInstanceOf(() => eval("({ get x(...a) { } })"), SyntaxError);
assertThrowsInstanceOf(() => eval("({ get x(a, ...b) { } })"), SyntaxError);
assertThrowsInstanceOf(() => eval("({ get x([a], ...b) { } })"), SyntaxError);
assertThrowsInstanceOf(() => eval("({ get x({a}, ...b) { } })"), SyntaxError);
assertThrowsInstanceOf(() => eval("({ get x({a: A}, ...b) { } })"), SyntaxError);

assertThrowsInstanceOf(() => eval("({ set x(...a) { } })"), SyntaxError);
assertThrowsInstanceOf(() => eval("({ set x(a, ...b) { } })"), SyntaxError);
assertThrowsInstanceOf(() => eval("({ set x([a], ...b) { } })"), SyntaxError);
assertThrowsInstanceOf(() => eval("({ set x({a: A}, ...b) { } })"), SyntaxError);

({ get(...a) { } });
({ get(a, ...b) { } });
({ get([a], ...b) { } });
({ get({a}, ...b) { } });
({ get({a: A}, ...b) { } });

({ set(...a) { } });
({ set(a, ...b) { } });
({ set([a], ...b) { } });
({ set({a: A}, ...b) { } });

