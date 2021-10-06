// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getoffsetstringfor
description: Fall back to calling intrinsic getOffsetNanosecondsFor() when method is deleted
features: [Temporal]
---*/

const instant = Temporal.Instant.from("1975-02-02T14:25:36.123456789Z");
const timeZone = Temporal.TimeZone.from("Europe/Madrid");
timeZone.getOffsetNanosecondsFor = undefined;
assert.sameValue(timeZone.getOffsetStringFor(instant), "+01:00");
