// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: ZonedDateTime strings with UTC offset fractional part are not confused with time fractional part
features: [Temporal]
---*/

const datetime = new Temporal.ZonedDateTime(29_012_345_679n, "+00:01:30.987654321");
const str = "1970-01-01T00:02:00.000000000+00:02[+00:01:30.987654321]";

assert.sameValue(Temporal.ZonedDateTime.compare(str, datetime), 0, "Time zone determined from bracket name (first argument)");
assert.sameValue(Temporal.ZonedDateTime.compare(datetime, str), 0, "Time zone determined from bracket name (second argument)");
