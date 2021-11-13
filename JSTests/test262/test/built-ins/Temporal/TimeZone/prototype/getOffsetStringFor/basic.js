// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Basic tests for Temporal.TimeZone.prototype.getOffsetStringFor
features: [BigInt, Temporal]
---*/

const instant = new Temporal.Instant(0n);

function test(timeZoneIdentifier, expectedOffsetString, description) {
  const timeZone = new Temporal.TimeZone(timeZoneIdentifier);
  assert.sameValue(timeZone.getOffsetStringFor(instant), expectedOffsetString, description);
}

test("UTC", "+00:00", "offset of UTC is +00:00");
test("+01:00", "+01:00", "positive offset");
test("-05:00", "-05:00", "negative offset");
test("+00:44:59.123456789", "+00:44:59.123456789", "sub-minute offset is not rounded");
