// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.from
description: various interesting string arguments.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const result = Temporal.PlainDate.from(19761118);
TemporalHelpers.assertPlainDate(result, 1976, 11, "M11", 18);
