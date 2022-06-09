// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.tostring
description: Rounding for fractionalSecondDigits option
info: |
    sec-getstringornumberoption step 3.b:
      b. Return floor(ℝ(_value_)).
    sec-temporal-tosecondsstringprecision step 9:
      9. Let _digits_ be ? GetStringOrNumberOption(_normalizedOptions_, *"fractionalSecondDigits"*, « *"auto"* », 0, 9, *"auto"*).
    sec-temporal.zoneddatetime.prototype.tostring step 4:
      4. Let _precision_ be ? ToDurationSecondsStringPrecision(_options_).
features: [Temporal]
---*/

const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_650_000n, "UTC");

const string = datetime.toString({ fractionalSecondDigits: 2.5 });
assert.sameValue(string, "2001-09-09T01:46:40.98+00:00[UTC]", "fractionalSecondDigits 2.5 floors to 2");
