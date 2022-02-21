// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.getisofields
description: Correct field names on the object returned from getISOFields
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);

const result = time.getISOFields();
assert.sameValue(result.isoHour, 12, "isoHour result");
assert.sameValue(result.isoMinute, 34, "isoMinute result");
assert.sameValue(result.isoSecond, 56, "isoSecond result");
assert.sameValue(result.isoMillisecond, 987, "isoMillisecond result");
assert.sameValue(result.isoMicrosecond, 654, "isoMicrosecond result");
assert.sameValue(result.isoNanosecond, 321, "isoNanosecond result");
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
