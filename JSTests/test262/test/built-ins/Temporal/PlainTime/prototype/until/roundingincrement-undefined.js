// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.until
description: Fallback value for roundingIncrement option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-totemporalroundingincrement step 5:
      5. Let _increment_ be ? GetOption(_normalizedOptions_, *"roundingIncrement"*, « Number », *undefined*, 1).
    sec-temporal.plaintime.prototype.until step 10:
      10. Let _roundingIncrement_ be ? ToTemporalRoundingIncrement(_options_, _maximum_, *false*).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
const later = new Temporal.PlainTime(13, 35, 57, 988, 655, 322);

const explicit = earlier.until(later, { roundingIncrement: undefined });
TemporalHelpers.assertDuration(explicit, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, "default roundingIncrement is 1");

const implicit = earlier.until(later, {});
TemporalHelpers.assertDuration(implicit, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, "default roundingIncrement is 1");
