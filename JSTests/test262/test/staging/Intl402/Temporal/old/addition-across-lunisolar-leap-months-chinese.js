// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Addition across lunisolar leap months
features: [Temporal]
---*/

const chineseYearOffset = new Temporal.PlainDate(1, 1, 1, "chinese").year;

// Adding years across Chinese leap month
var date = Temporal.PlainDate.from({
  year: 2000 + chineseYearOffset,
  monthCode: "M08",
  day: 2,
  calendar: "chinese"
});
var added = date.add({ years: 1 });
assert.sameValue(added.day, date.day);
assert.sameValue(added.monthCode, date.monthCode);
assert.sameValue(added.year, date.year + 1);

// Adding months across Chinese leap month
var date = Temporal.PlainDate.from({
  year: 2000 + chineseYearOffset,
  monthCode: "M08",
  day: 2,
  calendar: "chinese"
});
var added = date.add({ months: 13 });
assert.sameValue(added.day, date.day);
assert.sameValue(added.monthCode, date.monthCode);
assert.sameValue(added.year, date.year + 1);

// Adding months and years across Chinese leap month
var date = Temporal.PlainDate.from({
  year: 2001 + chineseYearOffset,
  monthCode: "M08",
  day: 2,
  calendar: "chinese"
});
var added = date.add({
  years: 1,
  months: 12
});
assert.sameValue(added.day, date.day);
assert.sameValue(added.monthCode, date.monthCode);
assert.sameValue(added.year, date.year + 2);
