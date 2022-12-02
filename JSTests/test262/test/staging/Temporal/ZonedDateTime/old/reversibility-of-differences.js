// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Reversibility of differences
features: [Temporal]
---*/

var earlier = Temporal.ZonedDateTime.from("1976-11-18T15:23:30.123456789-03:00[-03:00]");
var later = Temporal.ZonedDateTime.from("2019-10-29T10:46:38.271986102-03:00[-03:00]");
[
  "hours",
  "minutes",
  "seconds"
].forEach(largestUnit => {
  var diff = later.since(earlier, { largestUnit });
assert.sameValue(`${ earlier.since(later, { largestUnit }) }`, `${ diff.negated() }`);
assert.sameValue(`${ earlier.until(later, { largestUnit }) }`, `${ diff }`);
// difference symmetrical with regard to negative durations
    assert(earlier.subtract(diff.negated()).equals(later));
    assert(later.add(diff.negated()).equals(earlier));
  });
[
  "years",
  "months",
  "weeks",
  "days",
  "hours",
  "minutes",
  "seconds"
].forEach(largestUnit => {
  var diff1 = earlier.until(later, { largestUnit });
  var diff2 = later.since(earlier, { largestUnit });
  assert(earlier.add(diff1).equals(later));
  assert(later.subtract(diff2).equals(earlier));
});
