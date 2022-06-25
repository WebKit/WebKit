// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetimeiso
description: Leap second is a valid ISO string for TimeZone
features: [Temporal]
---*/

let timeZone = "2016-12-31T23:59:60+00:00[UTC]";

// A string with a leap second is a valid ISO string, so the following two
// operations should not throw

Temporal.Now.plainDateTimeISO(timeZone);
Temporal.Now.plainDateTimeISO({ timeZone });

timeZone = "2021-08-19T17:30:45.123456789+23:59[+23:59:60]";
assert.throws(RangeError, () => Temporal.Now.plainDateTimeISO(timeZone), "leap second in time zone name not valid");
assert.throws(RangeError, () => Temporal.Now.plainDateTimeISO({ timeZone }), "leap second in time zone name not valid (nested property)");
