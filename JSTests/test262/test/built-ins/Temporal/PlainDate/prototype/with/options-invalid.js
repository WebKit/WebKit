// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.with
description: TypeError thrown when a primitive is passed as the options argument
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(2000, 2, 2);
[null, true, "hello", Symbol("foo"), 1, 1n].forEach((badOptions) =>
  assert.throws(TypeError, () => plainDate.with({ day: 17 }, badOptions))
);
