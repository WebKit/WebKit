// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: Time zone names are case insensitive
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(); 

const timeZone = 'uTc';
const result = instance.toZonedDateTime({ plainDate: new Temporal.PlainDate(2000, 5, 2), timeZone });
assert.sameValue(result.timeZone.id, 'UTC', `Time zone created from string "${timeZone}"`);
