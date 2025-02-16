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
try {
    decodeURIComponent('%ED%A0%80');
    assert.sameValue(true, false, "expected an URIError");
} catch (e) {
  assert.sameValue(e instanceof URIError, true);
}
