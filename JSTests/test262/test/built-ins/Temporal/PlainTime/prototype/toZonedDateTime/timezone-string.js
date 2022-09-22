// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: Time zone IDs are valid input for a time zone
features: [Temporal]
---*/

const instance = new Temporal.PlainTime();

["UTC", "+01:30"].forEach((timeZone) => {
  const result = instance.toZonedDateTime({ plainDate: new Temporal.PlainDate(2000, 5, 2), timeZone });
  assert.sameValue(result.timeZone.id, timeZone, `Time zone created from string "${timeZone}"`);
});
