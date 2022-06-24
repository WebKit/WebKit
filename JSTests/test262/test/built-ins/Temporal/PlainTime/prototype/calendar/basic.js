// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaintime.prototype.calendar
description: calendar returns the iso8601 calendar.
features: [Temporal]
---*/

const pt = new Temporal.PlainTime();
assert(pt.calendar instanceof Temporal.Calendar, "getter returns Calendar object");
assert.sameValue(pt.calendar.toString(), "iso8601", "getter returns iso8601 calendar");
assert.sameValue(pt.calendar, pt.calendar, "getter returns the same object");
