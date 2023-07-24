// Copyright (C) 2023 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DurationFormat.prototype.formatToParts
description:  Checks basic handling of formatToParts, using long, short,narrow and digital styles.
features: [Intl.DurationFormat]
---*/

// Utils functions
function* zip(a, b) {
  for (let i = 0; i < a.length; ++i) {
    yield [i, a[i], b[i]];
  }
}

function compare(actual, expected, message) {
  assert.sameValue(Array.isArray(expected), true, `${message}: expected is Array`);
  assert.sameValue(Array.isArray(actual), true, `${message}: actual is Array`);
  assert.sameValue(actual.length, expected.length, `${message}: length`);

  for (const [i, actualEntry, expectedEntry] of zip(actual, expected)) {
    // assertions
    assert.sameValue(actualEntry.type, expectedEntry.type, `type for entry ${i}`);
    assert.sameValue(actualEntry.value, expectedEntry.value, `value for entry ${i}`);
    if (expectedEntry.unit) {
      assert.sameValue(actualEntry.unit, expectedEntry.unit, `unit for entry ${i}`);
    }
  }
}
const duration = {
  hours: 7,
  minutes: 8,
  seconds: 9,
  milliseconds: 123,
  microseconds: 456,
  nanoseconds: 789,
};

const style = "narrow";
const expected = [
    { type: "integer", value: "7", unit: "hour" },
    { type: "unit", value: "h", unit: "hour" },
    { type: "literal", value: " " },
    { type: "integer", value: "8", unit: "minute" },
    { type: "unit", value: "m", unit: "minute" },
    { type: "literal", value: " " },
    { type: "integer", value: "9", unit: "second" },
    { type: "unit", value: "s", unit: "second" },
    { type: "literal", value: " " },
    { type: "integer", value: "123", unit: "millisecond" },
    { type: "unit", value: "ms", unit: "millisecond" },
    { type: "literal", value: " " },
    { type: "integer", value: "456", unit: "microsecond" },
    { type: "unit", value: "μs", unit: "microsecond" },
    { type: "literal", value: " " },
    { type: "integer", value: "789", unit: "nanosecond" },
    { type: "unit", value: "ns", unit: "nanosecond" },
  ];

let df = new Intl.DurationFormat('en', { style });
compare(df.formatToParts(duration), expected, `Using style : ${style}`);
