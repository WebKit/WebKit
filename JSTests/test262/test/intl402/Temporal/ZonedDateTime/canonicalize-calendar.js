// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: Calendar ID is canonicalized
features: [Temporal]
---*/

const result = new Temporal.ZonedDateTime(1719923640_000_000_000n, "UTC", "islamicc");
assert.sameValue(result.calendarId, "islamic-civil", "calendar ID is canonicalized");
