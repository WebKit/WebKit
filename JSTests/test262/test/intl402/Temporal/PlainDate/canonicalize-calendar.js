// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate
description: Calendar ID is canonicalized
features: [Temporal]
---*/

const result = new Temporal.PlainDate(2024, 7, 2, "islamicc");
assert.sameValue(result.calendarId, "islamic-civil", "calendar ID is canonicalized");
