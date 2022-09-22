// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tozoneddatetime
description: Time zone IDs are valid input for a time zone
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(2000, 5, 2);

["UTC", "+01:30"].forEach((timeZone) => {
  const result = instance.toZonedDateTime(timeZone);
  assert.sameValue(result.timeZone.id, timeZone, `Time zone created from string "${timeZone}"`);
});
