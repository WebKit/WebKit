// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.withcalendar
description: RangeError thrown when calendar argument not given
features: [Temporal]
---*/

const plainDate = Temporal.PlainDate.from("1976-11-18");
assert.throws(RangeError, () => plainDate.withCalendar(), "missing argument");
assert.throws(RangeError, () => plainDate.withCalendar(undefined), "undefined argument");
