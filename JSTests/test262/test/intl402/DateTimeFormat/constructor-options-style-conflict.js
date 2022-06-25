// Copyright 2021 Kate Miháliková. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
    Conflicting properties of dateStyle/timeStyle must be rejected with a TypeError for the options argument to the DateTimeFormat constructor.
info: |
    InitializeDateTimeFormat ( dateTimeFormat, locales, options )

    ...
    36. If dateStyle is not undefined or timeStyle is not undefined, then
      a. For each row in Table 4, except the header row, do
        i. Let prop be the name given in the Property column of the row.
        ii. Let p be opt.[[<prop>]].
        iii. If p is not undefined, then
          1. Throw a TypeError exception.
---*/


// Table 4 - Property column + example value from Values column
const conflictingOptions = [
 [ "weekday", "short" ],
 [ "era", "short" ],
 [ "year", "numeric" ],
 [ "month", "numeric" ],
 [ "day", "numeric" ],
 [ "dayPeriod", "short" ],
 [ "hour", "numeric" ],
 [ "minute", "numeric" ],
 [ "second", "numeric" ],
 [ "fractionalSecondDigits", 3 ],
 [ "timeZoneName", "short" ],
];

for (const [ option, value ] of conflictingOptions) {
  assert.throws(TypeError, function() {
    new Intl.DateTimeFormat("en", { [option]: value, dateStyle: "short" });
  }, `new Intl.DateTimeFormat("en", { ${option}: "${value}",  dateStyle: "short" }) throws TypeError`);

  assert.throws(TypeError, function() {
    new Intl.DateTimeFormat("en", { [option]: value, timeStyle: "short" });
  }, `new Intl.DateTimeFormat("en", { ${option}: "${value}",  timeStyle: "short" }) throws TypeError`);
}
