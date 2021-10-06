// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getprevioustransition
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

const instance = new Temporal.TimeZone("America/Vancouver");

let str = "1970-01-01T00:00";
assert.throws(RangeError, () => instance.getPreviousTransition(str), "bare date-time string is not an instant");
str = "1970-01-01T00:00[America/Vancouver]";
assert.throws(RangeError, () => instance.getPreviousTransition(str), "date-time + IANA annotation is not an instant");

// The following are all valid strings so should not throw:

[
  "1970-01-01T00:00Z",
  "1970-01-01T00:00+01:00",
  "1970-01-01T00:00Z[America/Vancouver]",
  "1970-01-01T00:00+01:00[America/Vancouver]",
].forEach((str) => instance.getPreviousTransition(str));
