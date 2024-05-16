// Copyright (C) 2024 Jordan Harband. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Promise.try returns a Promise that rejects when the function throws
esid: sec-promise.try
features: [promise-try]
flags: [async]
---*/

assert.throwsAsync(
  Test262Error,
  function () {
    Promise.try(function () { throw new Test262Error(); })
  },
  "error thrown from callback must become a rejection"
);
