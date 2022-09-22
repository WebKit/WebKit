// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.from
description: Time zone IDs are valid input for a time zone
features: [Temporal]
---*/

["UTC", "+01:30"].forEach((timeZone) => {
  const result = Temporal.TimeZone.from(timeZone);
  assert.sameValue(result.id, timeZone, `Time zone created from string "${timeZone}"`);
});
