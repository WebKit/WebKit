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
// Constructors can't be called so we can't pattern match
// them in replace and sort.
function a() {
    var b = {a: "A"};

    class X {
        constructor(a) {
            return b[a]
        }
    };

    assertThrowsInstanceOf(() => "a".replace(/a/, X), TypeError);
}

function b() {
    class X {
        constructor(x, y) {
            return x - y;
        }
    }

    assertThrowsInstanceOf(() => [1, 2, 3].sort(X), TypeError);
}

a();
b();

