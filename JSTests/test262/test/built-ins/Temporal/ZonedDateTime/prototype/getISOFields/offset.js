// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.getisofields
description: The offset property of returned object
features: [BigInt, Temporal]
---*/

function test(timeZoneIdentifier, expectedOffsetString, description) {
  const timeZone = new Temporal.TimeZone(timeZoneIdentifier);
  const datetime = new Temporal.ZonedDateTime(0n, timeZone);
  const fields = datetime.getISOFields();
  assert.sameValue(fields.offset, expectedOffsetString, description);
}

test("UTC", "+00:00", "offset of UTC is +00:00");
test("+01:00", "+01:00", "positive offset");
test("-05:00", "-05:00", "negative offset");
