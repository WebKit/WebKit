// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.fields
description: TypeError thrown if the input iterable yields a non-String value
info: |
    sec-temporal.calendar.prototype.fields step 5:
      5. For each element _fieldName_ of _fieldNames_, do
        a. If Type(_fieldName_) is not String, throw a *TypeError* exception.
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
[true, 3, 3n, {}, () => {}, Symbol(), undefined, null].forEach((element) => {
  assert.throws(TypeError, () => calendar.fields([element]), "bad input to calendar.fields()");
});
