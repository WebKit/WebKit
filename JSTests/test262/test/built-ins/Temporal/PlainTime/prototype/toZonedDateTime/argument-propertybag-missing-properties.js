// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.tozoneddatetime
description: Both plainDate and timeZone properties need to not be undefined.
features: [Temporal]
---*/

const instance = new Temporal.PlainTime();
const plainDate = new Temporal.PlainDate(2022, 5, 19);
const timeZone = new Temporal.TimeZone("UTC");
assert.throws(TypeError, () => instance.toZonedDateTime({}),
  "no properties");
assert.throws(TypeError, () => instance.toZonedDateTime({ plainDate }),
  "only plainDate");
assert.throws(TypeError, () => instance.toZonedDateTime({ plainDate, timeZone: undefined }),
  "timeZone explicitly undefined");
assert.throws(TypeError, () => instance.toZonedDateTime({ timeZone }),
  "only timeZone");
assert.throws(TypeError, () => instance.toZonedDateTime({ plainDate: undefined, timeZone }),
  "plainDate explicitly undefined");
