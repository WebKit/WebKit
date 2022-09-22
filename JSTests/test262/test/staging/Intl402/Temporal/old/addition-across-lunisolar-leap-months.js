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
assert.sameValue(added.monthCode, date.monthCode);
assert.sameValue(added.year, date.year + 2);
var testChineseData = new Date("2001-02-01T00:00Z").toLocaleString("en-US-u-ca-chinese", {
  day: "numeric",
  month: "numeric",
  year: "numeric",
  era: "short",
  timeZone: "UTC"
});
var hasOutdatedChineseIcuData = !testChineseData.endsWith("2001");

// Adding years across Chinese leap month"
if(hasOutdatedChineseIcuData) {
  var date = Temporal.PlainDate.from({
    year: 2000,
    monthCode: "M08",
    day: 2,
    calendar: "chinese"
  });
  var added = date.add({ years: 1 });
  assert.sameValue(added.monthCode, date.monthCode);
  assert.sameValue(added.year, date.year + 1);
}
// Adding months across Chinese leap month
if(hasOutdatedChineseIcuData) {
  var date = Temporal.PlainDate.from({
    year: 2000,
    monthCode: "M08",
    day: 2,
    calendar: "chinese"
  });
  var added = date.add({ months: 13 });
  assert.sameValue(added.monthCode, date.monthCode);
  assert.sameValue(added.year, date.year + 1);
}

// Adding months and years across Chinese leap month
if(hasOutdatedChineseIcuData) {
  var date = Temporal.PlainDate.from({
    year: 2001,
    monthCode: "M08",
    day: 2,
    calendar: "chinese"
  });
  var added = date.add({
    years: 1,
    months: 12
  });
  assert.sameValue(added.monthCode, date.monthCode);
  assert.sameValue(added.year, date.year + 2);
};
