// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-temporaldurationtostring
description: >
  Duration components are precise mathematical integers.
info: |
  TemporalDurationToString ( years, months, weeks, days, hours, minutes, seconds, milliseconds,
                             microseconds, nanoseconds, precision )
  ...
  2. Set microseconds to microseconds + RoundTowardsZero(nanoseconds / 1000).
  3. Set nanoseconds to remainder(nanoseconds, 1000).
  4. Set milliseconds to milliseconds + RoundTowardsZero(microseconds / 1000).
  5. Set microseconds to remainder(microseconds, 1000).
  6. Set seconds to seconds + RoundTowardsZero(milliseconds / 1000).
  7. Set milliseconds to remainder(milliseconds, 1000).
  ...
features: [Temporal]
---*/

{
  let d = Temporal.Duration.from({microseconds: 10000000000000004000, nanoseconds: 1000});

  // "PT10000000000000.004096S" with float64.
  // "PT10000000000000.004097S" with exact precision.
  assert.sameValue(d.toString(), "PT10000000000000.004097S");
}

{
  let d = Temporal.Duration.from({seconds: 10000000000000004000, microseconds: 1000});

  // "PT10000000000000004000.001S" with float64.
  // "PT10000000000000004096.001S" with exact precision.
  assert.sameValue(d.toString(), "PT10000000000000004096.001S");
}
