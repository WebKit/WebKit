// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.tostring
description: RangeError thrown when fractionalSecondDigits option not one of the allowed string values
info: |
    sec-getstringornumberoption step 4:
      4. If _stringValues_ is not *undefined* and _stringValues_ does not contain an element equal to _value_, throw a *RangeError* exception.
    sec-temporal-tosecondsstringprecision step 9:
      9. Let _digits_ be ? GetStringOrNumberOption(_normalizedOptions_, *"fractionalSecondDigits"*, « *"auto"* », 0, 9, *"auto"*).
    sec-temporal.instant.prototype.tostring step 4:
      4. Let _precision_ be ? ToDurationSecondsStringPrecision(_options_).
features: [Temporal]
---*/

const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_650_000n, "UTC");

assert.throws(RangeError, () => datetime.toString({ fractionalSecondDigits: "other string" }));
