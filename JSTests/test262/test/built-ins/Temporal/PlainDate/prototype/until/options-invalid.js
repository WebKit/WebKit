// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: TypeError thrown when a primitive is passed as the options argument
features: [Temporal]
---*/

const feb20 = Temporal.PlainDate.from("2020-02-01");
const feb21 = Temporal.PlainDate.from("2021-02-01");
const values = [null, true, "hello", Symbol("foo"), 1, 1n];
for (const badOptions of values) {
  assert.throws(TypeError, () => feb20.until(feb21, badOptions));
}
