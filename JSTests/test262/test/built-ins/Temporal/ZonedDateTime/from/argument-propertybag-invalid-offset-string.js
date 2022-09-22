// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Property bag with offset property is rejected if offset is in the wrong format
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");

const offsetOptions = ['use', 'prefer', 'ignore', 'reject'];

const badOffsets = [
  "00:00",    // missing sign
  "+0",       // too short
  "-000:00",  // too long
  0,          // converts to a string that is invalid
];
offsetOptions.forEach((offsetOption) => {
  badOffsets.forEach((offset) => {
    const arg = { year: 2021, month: 10, day: 28, offset, timeZone };
    assert.throws(
      RangeError,
      () => Temporal.ZonedDateTime.from(arg, { offset: offsetOption }),
      `"${offset} is not a valid offset string (with offset option ${offsetOption})`
    );
  });
});
