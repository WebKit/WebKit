// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Japanese eras
features: [Temporal]
---*/


// Reiwa (2019-)
var date = Temporal.PlainDate.from({
  era: "reiwa",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "2020-01-01[u-ca=japanese]");

// Heisei (1989-2019)
var date = Temporal.PlainDate.from({
  era: "heisei",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1990-01-01[u-ca=japanese]");

// Showa (1926-1989)
var date = Temporal.PlainDate.from({
  era: "showa",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1927-01-01[u-ca=japanese]");

// Taisho (1912-1926)
var date = Temporal.PlainDate.from({
  era: "taisho",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1913-01-01[u-ca=japanese]");

// Meiji (1868-1912)
var date = Temporal.PlainDate.from({
  era: "meiji",
  eraYear: 2,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1869-01-01[u-ca=japanese]");

// Dates in same year before Japanese era starts will resolve to previous era
var date = Temporal.PlainDate.from({
  era: "reiwa",
  eraYear: 1,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "2019-01-01[u-ca=japanese]");
assert.sameValue(date.era, "heisei");
assert.sameValue(date.eraYear, 31);
date = Temporal.PlainDate.from({
  era: "heisei",
  eraYear: 1,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1989-01-01[u-ca=japanese]");
assert.sameValue(date.era, "showa");
assert.sameValue(date.eraYear, 64);
date = Temporal.PlainDate.from({
  era: "showa",
  eraYear: 1,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1926-01-01[u-ca=japanese]");
assert.sameValue(date.era, "taisho");
assert.sameValue(date.eraYear, 15);
date = Temporal.PlainDate.from({
  era: "taisho",
  eraYear: 1,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1912-01-01[u-ca=japanese]");
assert.sameValue(date.era, "meiji");
assert.sameValue(date.eraYear, 45);
date = Temporal.PlainDate.from({
  era: "meiji",
  eraYear: 1,
  month: 1,
  day: 1,
  calendar: "japanese"
});
assert.sameValue(`${ date }`, "1868-01-01[u-ca=japanese]");
assert.sameValue(date.era, "ce");
assert.sameValue(date.eraYear, 1868);
assert.throws(RangeError, () => Temporal.PlainDate.from({
  era: "bce",
  eraYear: 1,
  month: 1,
  day: 1,
  calendar: "japanese"
}));

// `with` doesn't crash when constraining dates out of bounds of the current era
var date = Temporal.PlainDate.from("1989-01-07").withCalendar(Temporal.Calendar.from("japanese")).with({ day: 10 });
assert.sameValue(`${ date }`, "1989-01-10[u-ca=japanese]");
