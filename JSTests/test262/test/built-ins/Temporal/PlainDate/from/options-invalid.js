// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: TypeError thrown when a primitive is passed as the options argument
features: [Temporal]
---*/

const fields = { year: 2000, month: 13, day: 2 };

const values = [null, true, "hello", Symbol("foo"), 1, 1n];
for (const badOptions of values) {
  assert.throws(TypeError, () => Temporal.PlainDate.from(fields, badOptions));
}
