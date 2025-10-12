// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Japanese eras
features: [Temporal]
---*/

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
