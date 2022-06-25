// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: RangeError thrown when calendar argument not given
features: [Temporal]
---*/

const plainDateTime = Temporal.PlainDateTime.from("1976-11-18T14:00:00");
assert.throws(RangeError, () => plainDateTime.withCalendar(), "missing argument");
assert.throws(RangeError, () => plainDateTime.withCalendar(undefined), "undefined argument");
