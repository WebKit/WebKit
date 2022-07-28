// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.from
description: String arguments at the limit of the representable range
features: [Temporal]
---*/

const minInstant = new Temporal.Instant(-86400_00000000_000_000_000n);
const maxInstant = new Temporal.Instant(86400_00000000_000_000_000n);

const minInstantStrings = [
  "-271821-04-20T00:00Z",
  "-271821-04-19T23:00-01:00",
  "-271821-04-19T00:00:00.000000001-23:59:59.999999999",
];
for (const str of minInstantStrings) {
  assert.sameValue(Temporal.Instant.from(str).epochNanoseconds, minInstant.epochNanoseconds, `instant string ${str} should be valid`);
}

const maxInstantStrings = [
  "+275760-09-13T00:00Z",
  "+275760-09-13T01:00+01:00",
  "+275760-09-13T23:59:59.999999999+23:59:59.999999999",
];

for (const str of maxInstantStrings) {
  assert.sameValue(Temporal.Instant.from(str).epochNanoseconds, maxInstant.epochNanoseconds, `instant string ${str} should be valid`);
}

const outOfRangeInstantStrings = [
  "-271821-04-19T23:59:59.999999999Z",
  "-271821-04-19T23:00-00:59:59.999999999",
  "-271821-04-19T00:00:00-23:59:59.999999999",
  "-271821-04-19T00:00:00-24:00",
  "+275760-09-13T00:00:00.000000001Z",
  "+275760-09-13T01:00+00:59:59.999999999",
  "+275760-09-14T00:00+23:59:59.999999999",
  "+275760-09-14T00:00+24:00",
];

for (const str of outOfRangeInstantStrings) {
  assert.throws(RangeError, () => Temporal.Instant.from(str), `instant string ${str} should not be valid`);
}
