// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-constructor
description: >
  The FinalizationGroup constructor is the %FinalizationGroup% intrinsic object and the initial
  value of the FinalizationGroup property of the global object.
features: [FinalizationGroup]
---*/

assert.sameValue(
  typeof FinalizationGroup, 'function',
  'typeof FinalizationGroup is function'
);
