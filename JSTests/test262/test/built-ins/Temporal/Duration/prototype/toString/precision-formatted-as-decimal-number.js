// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-temporaldurationtostring
description: >
  Duration components are formatted as precise decimal numbers.
info: |
  TemporalDurationToString ( years, months, weeks, days, hours, minutes, seconds, milliseconds,
                             microseconds, nanoseconds, precision )
  ...
  9. If years is not 0, then
    a. Set datePart to the string concatenation of abs(years) formatted as a decimal number and
       the code unit 0x0059 (LATIN CAPITAL LETTER Y).
  10. If months is not 0, then
    a. Set datePart to the string concatenation of datePart, abs(months) formatted as a decimal
       number, and the code unit 0x004D (LATIN CAPITAL LETTER M).
  If weeks is not 0, then
    a. Set datePart to the string concatenation of datePart, abs(weeks) formatted as a decimal
       number, and the code unit 0x0057 (LATIN CAPITAL LETTER W).
  ...
features: [Temporal]
---*/

{
  let d = Temporal.Duration.from({weeks: 10000000000000004000});

  // Number(10000000000000004000).toString() is "10000000000000004000".
  assert.sameValue(d.toString(), "P10000000000000004096W");
}

{
  let d = Temporal.Duration.from({months: 9e59});

  // Number(9e+59).toString() is "9e+59".
  assert.sameValue(d.toString(), "P899999999999999918767229449717619953810131273674690656206848M");
}

{
  let d = Temporal.Duration.from({years: Number.MAX_VALUE});

  // Number.MAX_VALUE.toString() is "1.7976931348623157e+308".
  assert.sameValue(d.toString(), "P179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368Y");
}
