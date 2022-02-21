// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: A negative duration result is balanced correctly by the modulo operation in NanosecondsToDays
info: |
    sec-temporal-nanosecondstodays step 6:
      6. If Type(_relativeTo_) is not Object or _relativeTo_ does not have an [[InitializedTemporalZonedDateTime]] internal slot, then
        a. Return the new Record { ..., [[Nanoseconds]]: abs(_nanoseconds_) modulo _dayLengthNs_ × _sign_, ... }.
    sec-temporal-balanceduration step 4:
      4. If _largestUnit_ is one of *"year"*, *"month"*, *"week"*, or *"day"*, then
        a. Let _result_ be ? NanosecondsToDays(_nanoseconds_, _relativeTo_).
    sec-temporal-addduration steps 5–6:
      5. If _relativeTo_ is *undefined*, then
        ...
        b. Let _result_ be ! BalanceDuration(_d1_ + _d2_, _h1_ + _h2_, _min1_ + _min2_, _s1_ + _s2_, _ms1_ + _ms2_, _mus1_ + _mus2_, _ns1_ + _ns2_, _largestUnit_).
        ...
      6. Else if _relativeTo_ has an [[InitializedTemporalPlainDateTime]] internal slot, then
        ...
        n. Let _result_ be ! BalanceDuration(_dateDifference_.[[Days]], _h1_ + _h2_, _min1_ + _min2_, _s1_ + _s2_, _ms1_ + _ms2_, _mus1_ + _mus2_, _ns1_ + _ns2_, _largestUnit_).
    sec-temporal.duration.prototype.subtract step 6:
      6. Let _result_ be ? AddDuration(_duration_.[[Years]], _duration_.[[Months]], _duration_.[[Weeks]], _duration_.[[Days]], _duration_.[[Hours]], _duration_.[[Minutes]], _duration_.[[Seconds]], _duration_.[[Milliseconds]], _duration_.[[Microseconds]], _duration_.[[Nanoseconds]], −_other_.[[Years]], −_other_.[[Months]], −_other_.[[Weeks]], −_other_.[[Days]], −_other_.[[Hours]], −_other_.[[Minutes]], −_other_.[[Seconds]], −_other_.[[Milliseconds]], −_other_.[[Microseconds]], −_other_.[[Nanoseconds]], _relativeTo_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const duration1 = new Temporal.Duration(0, 0, 0, 0, -60);
const duration2 = new Temporal.Duration(0, 0, 0, -1);

const resultNotRelative = duration1.subtract(duration2);
TemporalHelpers.assertDuration(resultNotRelative, 0, 0, 0, -1, -12, 0, 0, 0, 0, 0);

const relativeTo = new Temporal.PlainDateTime(2000, 1, 1);
const resultRelative = duration1.subtract(duration2, { relativeTo });
TemporalHelpers.assertDuration(resultRelative, 0, 0, 0, -1, -12, 0, 0, 0, 0, 0);
