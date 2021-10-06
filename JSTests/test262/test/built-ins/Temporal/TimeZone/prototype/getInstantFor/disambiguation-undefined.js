// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Fallback value for disambiguation option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-totemporaldisambiguation step 1:
      1. Return ? GetOption(_normalizedOptions_, *"disambiguation"*, « String », « *"compatible"*, *"earlier"*, *"later"*, *"reject"* », *"compatible"*).
    sec-temporal.timezone.prototype.getinstantfor step 5:
      5. Let _disambiguation_ be ? ToTemporalDisambiguation(_options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const timeZone = TemporalHelpers.springForwardFallBackTimeZone();
const springForwardDateTime = new Temporal.PlainDateTime(2000, 4, 2, 2, 30);
const fallBackDateTime = new Temporal.PlainDateTime(2000, 10, 29, 1, 30);

[
  [springForwardDateTime, 954671400_000_000_000n],
  [fallBackDateTime, 972808200_000_000_000n],
].forEach(([datetime, expected]) => {
  const explicit = timeZone.getInstantFor(datetime, { disambiguation: undefined });
  assert.sameValue(explicit.epochNanoseconds, expected, "default disambiguation is compatible");
  const implicit = timeZone.getInstantFor(datetime, {});
  assert.sameValue(implicit.epochNanoseconds, expected, "default disambiguation is compatible");
  const lambda = timeZone.getInstantFor(datetime, () => {});
  assert.sameValue(lambda.epochNanoseconds, expected, "default disambiguation is compatible");
});
