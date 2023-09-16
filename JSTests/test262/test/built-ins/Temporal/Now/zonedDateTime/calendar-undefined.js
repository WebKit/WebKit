// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetime
description: Throws when the calendar argument is undefined
features: [Temporal]
---*/

assert.throws(TypeError, () => Temporal.Now.zonedDateTime(), "implicit");
assert.throws(TypeError, () => Temporal.Now.zonedDateTime(undefined), "implicit");
