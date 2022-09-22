// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: America/Los_Angeles
features: [Temporal]
---*/

var zone = new Temporal.TimeZone("America/Los_Angeles");
var inst = new Temporal.Instant(0n);
var dtm = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789);
assert.sameValue(zone.id, `${ zone }`)
assert.sameValue(zone.getOffsetNanosecondsFor(inst), -8 * 3600000000000)
assert.sameValue(zone.getOffsetStringFor(inst), "-08:00")
assert(zone.getInstantFor(dtm) instanceof Temporal.Instant)
for (var i = 0, txn = inst; i < 4; i++) {
  var transition = zone.getNextTransition(txn);
  assert(transition instanceof Temporal.Instant);
  assert(!transition.equals(txn));
  txn = transition;
}
for (var i = 0, txn = inst; i < 4; i++) {
  var transition = zone.getPreviousTransition(txn);
  assert(transition instanceof Temporal.Instant);
  assert(!transition.equals(txn));
  txn = transition;
}
