// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: Tests for compare() with each possible outcome
features: [Temporal]
---*/

const cal1 = "iso8601";
const cal2 = new (class extends Temporal.Calendar { id = "custom"; })("iso8601");

assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal1),
    new Temporal.PlainDateTime(1987, 5, 31, 12, 15, 45, 333, 777, 111, cal2)
  ),
  1,
  "year >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 12, 15, 6, 30, 15, 222, 444, 6, cal1),
    new Temporal.PlainDateTime(2048, 12, 15, 6, 30, 15, 222, 444, 6, cal2)
  ),
  -1,
  "year <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal1),
    new Temporal.PlainDateTime(2000, 3, 31, 12, 15, 45, 333, 777, 111, cal2)
  ),
  1,
  "month >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 444, 6, cal1),
    new Temporal.PlainDateTime(1981, 12, 15, 6, 30, 15, 222, 444, 6, cal2)
  ),
  -1,
  "month <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal1),
    new Temporal.PlainDateTime(2000, 5, 14, 12, 15, 45, 333, 777, 111, cal2)
  ),
  1,
  "day >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 444, 6, cal1),
    new Temporal.PlainDateTime(1981, 4, 21, 6, 30, 15, 222, 444, 6, cal2)
  ),
  -1,
  "day <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal1),
    new Temporal.PlainDateTime(2000, 5, 31, 6, 15, 45, 333, 777, 111, cal2)
  ),
  1,
  "hour >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 444, 6, cal1),
    new Temporal.PlainDateTime(1981, 4, 15, 22, 30, 15, 222, 444, 6, cal2)
  ),
  -1,
  "hour <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal1),
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 22, 333, 777, 111, cal2)
  ),
  1,
  "minute >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 444, 6, cal1),
    new Temporal.PlainDateTime(1981, 4, 15, 6, 57, 15, 222, 444, 6, cal2)
  ),
  -1,
  "minute <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 6, 333, 777, 111, cal1),
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 5, 333, 777, 111, cal2)
  ),
  1,
  "second >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 3, 222, 444, 6, cal1),
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 4, 222, 444, 6, cal2)
  ),
  -1,
  "second <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 6, 777, 111, cal1),
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 5, 777, 111, cal2)
  ),
  1,
  "millisecond >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 3, 444, 6, cal1),
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 4, 444, 6, cal2)
  ),
  -1,
  "millisecond <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 6, 111, cal1),
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 5, 111, cal2)
  ),
  1,
  "microsecond >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 3, 6, cal1),
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 4, 6, cal2)
  ),
  -1,
  "microsecond <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 999, cal1),
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal2)
  ),
  1,
  "nanosecond >"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 444, 0, cal1),
    new Temporal.PlainDateTime(1981, 4, 15, 6, 30, 15, 222, 444, 6, cal2)
  ),
  -1,
  "nanosecond <"
);
assert.sameValue(
  Temporal.PlainDateTime.compare(
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal1),
    new Temporal.PlainDateTime(2000, 5, 31, 12, 15, 45, 333, 777, 111, cal2)
  ),
  0,
  "="
);
