// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.tostring
description: RangeError thrown when timeZoneName option not one of the allowed string values
info: |
    sec-getoption step 10:
      10. If _values_ is not *undefined* and _values_ does not contain an element equal to _value_, throw a *RangeError* exception.
    sec-temporal-toshowtimezonenameoption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"timeZoneName"*, « String », « *"auto"*, *"never"* », *"auto"*).
    sec-temporal.zoneddatetime.protoype.tostring step 7:
      7. Let _showTimeZone_ be ? ToShowTimeZoneNameOption(_options_).
features: [Temporal]
---*/

const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_64_321n, "UTC");
assert.throws(RangeError, () => datetime.toString({ timeZoneName: "other string" }));
