// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplaindatetime
description: Test of basic functionality for an exact time earlier than the Unix epoch
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const zdt = Temporal.ZonedDateTime.from("1969-07-16T13:32:01.234567891Z[-04:00]");
const dateTime = zdt.toPlainDateTime();
TemporalHelpers.assertPlainDateTime(dateTime, 1969, 7, "M07", 16, 9, 32, 1, 234, 567, 891);
