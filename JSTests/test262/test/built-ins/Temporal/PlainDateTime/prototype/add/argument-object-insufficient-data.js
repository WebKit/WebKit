// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.add
description: At least one recognized property has to be present in argument
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const jan31 = new Temporal.PlainDateTime(2020, 1, 31, 15, 0);

assert.throws(
  TypeError,
  () => jan31.add({}),
  "empty object not acceptable"
);

assert.throws(
  TypeError,
  () => jan31.add({ month: 12 }), // should be "months"
  "misspelled property in argument throws if no other properties are present"
);

assert.throws(
  TypeError,
  () => jan31.add({ nonsense: true }),
  "unrecognized properties throw if no other recognized property is present"
);

TemporalHelpers.assertPlainDateTime(
  jan31.add({ nonsense: 1, days: 1 }),
  2020, 2, "M02", 1, 15, 0, 0, 0, 0, 0,
  "unrecognized properties ignored provided at least one recognized property is present"
);
