// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: RangeError thrown when disambiguation option not one of the allowed string values
info: |
    sec-getoption step 10:
      10. If _values_ is not *undefined* and _values_ does not contain an element equal to _value_, throw a *RangeError* exception.
    sec-temporal-totemporaldisambiguation step 1:
      1. Return ? GetOption(_normalizedOptions_, *"disambiguation"*, « String », « *"compatible"*, *"earlier"*, *"later"*, *"reject"* », *"compatible"*).
    sec-temporal.timezone.prototype.getinstantfor step 5:
      5. Let _disambiguation_ be ? ToTemporalDisambiguation(_options_).
features: [Temporal]
---*/

const datetime = new Temporal.PlainDateTime(2001, 9, 9, 1, 46, 40, 987, 654, 321);
const timeZone = new Temporal.TimeZone("UTC");
assert.throws(RangeError, () => timeZone.getInstantFor(datetime, { disambiguation: "other string" }));
