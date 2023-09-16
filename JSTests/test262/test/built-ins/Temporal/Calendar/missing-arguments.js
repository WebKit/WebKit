// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar
description: RangeError thrown when constructor invoked with no argument
features: [Temporal]
---*/

assert.throws(TypeError, () => new Temporal.Calendar());
assert.throws(TypeError, () => new Temporal.Calendar(undefined));
