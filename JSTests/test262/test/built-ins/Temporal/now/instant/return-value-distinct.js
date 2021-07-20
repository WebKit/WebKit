// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.instant
description: Each invocation of the function produces a distinct object value
features: [Temporal]
---*/

var instant1 = Temporal.now.instant();
var instant2 = Temporal.now.instant();

assert.notSameValue(instant1, instant2);
