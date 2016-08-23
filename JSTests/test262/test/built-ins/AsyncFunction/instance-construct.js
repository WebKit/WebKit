// Copyright 2016 Microsoft, Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Brian Terlson <brian.terlson@microsoft.com>
esid: pending
description: >
  Async function instances are not constructors and do not have a
  [[Construct]] slot.
---*/

async function foo() { }
assert.throws(TypeError, function() {
  new foo();
});

