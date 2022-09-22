// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: Temporal.Instant.add works
features: [Temporal]
---*/

var inst = Temporal.Instant.from("1969-12-25T12:23:45.678901234Z");

// cross epoch in ms
var one = inst.subtract({
  hours: 240,
  nanoseconds: 800
});
var two = inst.add({
  hours: 240,
  nanoseconds: 800
});
var three = two.subtract({
  hours: 480,
  nanoseconds: 1600
});
var four = one.add({
  hours: 480,
  nanoseconds: 1600
});
assert.sameValue(`${ one }`, "1969-12-15T12:23:45.678900434Z", `(${ inst }).subtract({ hours: 240, nanoseconds: 800 }) = ${ one }`);
assert.sameValue(`${ two }`, "1970-01-04T12:23:45.678902034Z", `(${ inst }).add({ hours: 240, nanoseconds: 800 }) = ${ two }`);
assert(three.equals(one), `(${ two }).subtract({ hours: 480, nanoseconds: 1600 }) = ${ one }`);
assert(four.equals(two), `(${ one }).add({ hours: 480, nanoseconds: 1600 }) = ${ two }`);
