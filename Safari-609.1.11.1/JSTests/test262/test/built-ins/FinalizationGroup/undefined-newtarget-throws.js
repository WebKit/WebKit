// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-target
description: >
  Throws a TypeError if NewTarget is undefined.
info: |
  FinalizationGroup ( cleanupCallback )

  1. If NewTarget is undefined, throw a TypeError exception.
  2. If IsCallable(cleanupCallback) is false, throw a TypeError exception.
  ...
features: [FinalizationGroup]
---*/

assert.sameValue(
  typeof FinalizationGroup, 'function',
  'typeof FinalizationGroup is function'
);

assert.throws(TypeError, function() {
  FinalizationGroup();
});

assert.throws(TypeError, function() {
  FinalizationGroup(function() {});
});
