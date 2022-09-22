// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.from
description: A calendar ID is valid input for Calendar
features: [Temporal]
---*/

const arg = "iso8601";

const result = Temporal.Calendar.from(arg);
assert.sameValue(result.id, "iso8601", `Calendar created from string "${arg}"`);
