// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: Fallback value for fractionalSecondDigits option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-getstringornumberoption step 2:
      2. Let _value_ be ? GetOption(_options_, _property_, *"stringOrNumber"*, *undefined*, _fallback_).
    sec-temporal-tosecondsstringprecision step 9:
      9. Let _digits_ be ? GetStringOrNumberOption(_normalizedOptions_, *"fractionalSecondDigits"*, « *"auto"* », 0, 9, *"auto"*).
    sec-temporal.instant.prototype.tostring step 6:
      6. Let _precision_ be ? ToDurationSecondsStringPrecision(_options_).
features: [Temporal]
---*/

const instant = new Temporal.Instant(1_000_000_000_987_650_000n);

const explicit = instant.toString({ fractionalSecondDigits: undefined });
assert.sameValue(explicit, "2001-09-09T01:46:40.98765Z", "default fractionalSecondDigits is auto");

const implicit = instant.toString({});
assert.sameValue(implicit, "2001-09-09T01:46:40.98765Z", "default fractionalSecondDigits is auto");
