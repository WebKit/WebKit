// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetime
description: Time zone names are case insensitive
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n); 

const timeZone = 'uTc';
const result = instance.toZonedDateTime({ timeZone, calendar: "iso8601" });
assert.sameValue(result.timeZone.id, 'UTC', `Time zone created from string "${timeZone}"`);
