// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: >
  Call BalanceISOYearMonth with Number.MAX_VALUE and -Number.MAX_VALUE for years/months.
info: |
  Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )

  ...
  9. Let result be ? AddISODate(date.[[ISOYear]], date.[[ISOMonth]], date.[[ISODay]],
     duration.[[Years]], duration.[[Months]], duration.[[Weeks]], balanceResult.[[Days]],
     overflow).
  10. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]], result.[[Day]], calendar).

  AddISODate ( year, month, day, years, months, weeks, days, overflow )

  ...
  3. Let intermediate be ! BalanceISOYearMonth(year + years, month + months).
  ...

features: [Temporal]
---*/

var cal = new Temporal.Calendar("iso8601");
var date = new Temporal.PlainDate(1970, 1, 1);

var maxValue = new Temporal.Duration(Number.MAX_VALUE, Number.MAX_VALUE);
var minValue = new Temporal.Duration(-Number.MAX_VALUE, -Number.MAX_VALUE);

assert.throws(RangeError, () => cal.dateAdd(date, maxValue), "years/months is +Number.MAX_VALUE");
assert.throws(RangeError, () => cal.dateAdd(date, minValue), "years/months is -Number.MAX_VALUE");
