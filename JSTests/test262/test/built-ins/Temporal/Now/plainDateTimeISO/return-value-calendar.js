// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetimeiso
description: Temporal.Now.plainDateTimeISO is extensible.
features: [Temporal]
---*/

const result = Temporal.Now.plainDateTimeISO();
assert.sameValue(result.getISOFields().calendar, "iso8601", "calendar slot should store a string");
