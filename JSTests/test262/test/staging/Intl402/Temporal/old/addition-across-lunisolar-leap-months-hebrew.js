// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Addition across lunisolar leap months
features: [Temporal]
---*/

// Adding years across Hebrew leap month
var date = Temporal.PlainDate.from({
  year: 5783,
  monthCode: "M08",
  day: 2,
  calendar: "hebrew"
});
var added = date.add({ years: 1 });
assert.sameValue(added.day, date.day);
assert.sameValue(added.monthCode, date.monthCode);
assert.sameValue(added.year, date.year + 1);

// Adding months across Hebrew leap month
var date = Temporal.PlainDate.from({
  year: 5783,
  monthCode: "M08",
  day: 2,
  calendar: "hebrew"
});
var added = date.add({ months: 13 });
assert.sameValue(added.day, date.day);
assert.sameValue(added.monthCode, date.monthCode);
assert.sameValue(added.year, date.year + 1);

// Adding months and years across Hebrew leap month
var date = Temporal.PlainDate.from({
  year: 5783,
  monthCode: "M08",
  day: 2,
  calendar: "hebrew"
});
var added = date.add({
  years: 1,
  months: 12
});
assert.sameValue(added.day, date.day);
assert.sameValue(added.monthCode, date.monthCode);
assert.sameValue(added.year, date.year + 2);
