// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: Throws if a ZonedDateTime-like relativeTo string has the wrong UTC offset
features: [Temporal]
---*/

const duration1 = new Temporal.Duration(0, 0, 0, 31);
const duration2 = new Temporal.Duration(0, 1);
const relativeTo = "2000-01-01T00:00+05:30[UTC]";
assert.throws(RangeError, () => Temporal.Duration.compare(duration1, duration2, { relativeTo }));
