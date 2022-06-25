// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: Throws when the calendar argument is undefined
features: [Temporal]
---*/

assert.throws(RangeError, () => Temporal.Now.plainDateTime(), "implicit");
assert.throws(RangeError, () => Temporal.Now.plainDateTime(undefined), "implicit");
