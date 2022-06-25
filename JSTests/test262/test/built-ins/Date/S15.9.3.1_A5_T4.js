// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
info: |
    The [[Value]] property of the newly constructed object
    is set by following steps:
    8. If Result(1) is not NaN and 0 <= ToInteger(Result(1)) <= 99, Result(8) is
    1900+ToInteger(Result(1)); otherwise, Result(8) is Result(1)
    9. Compute MakeDay(Result(8), Result(2), Result(3))
    10. Compute MakeTime(Result(4), Result(5), Result(6), Result(7))
    11. Compute MakeDate(Result(9), Result(10))
    12. Set the [[Value]] property of the newly constructed object to
    TimeClip(UTC(Result(11)))
esid: sec-date-year-month-date-hours-minutes-seconds-ms
description: 5 arguments, (year, month, date, hours, minutes)
includes: [assertRelativeDateMs.js]
---*/

assertRelativeDateMs(new Date(1899, 11, 31, 23, 59), -2208988860000);

assertRelativeDateMs(new Date(1899, 12, 1, 0, 0), -2208988800000);

assertRelativeDateMs(new Date(1900, 0, 1, 0, 0), -2208988800000);

assertRelativeDateMs(new Date(1969, 11, 31, 23, 59), -60000);

assertRelativeDateMs(new Date(1969, 12, 1, 0, 0), 0);

assertRelativeDateMs(new Date(1970, 0, 1, 0, 0), 0);

assertRelativeDateMs(new Date(1999, 11, 31, 23, 59), 946684740000);

assertRelativeDateMs(new Date(1999, 12, 1, 0, 0), 946684800000);

assertRelativeDateMs(new Date(2000, 0, 1, 0, 0), 946684800000);

assertRelativeDateMs(new Date(2099, 11, 31, 23, 59), 4102444740000);

assertRelativeDateMs(new Date(2099, 12, 1, 0, 0), 4102444800000);

assertRelativeDateMs(new Date(2100, 0, 1, 0, 0), 4102444800000);
