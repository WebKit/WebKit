// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-timezone-objects
description: UTC
features: [Temporal]
---*/

var zone = new Temporal.TimeZone("UTC");
var inst = Temporal.Instant.fromEpochSeconds(Math.floor(Math.random() * 1000000000));
var dtm = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789);
assert.sameValue(zone.id, `${ zone }`)
assert.sameValue(zone.getOffsetNanosecondsFor(inst), 0)
assert(zone.getPlainDateTimeFor(inst) instanceof Temporal.PlainDateTime)
assert(zone.getInstantFor(dtm) instanceof Temporal.Instant)
assert.sameValue(zone.getNextTransition(inst), null)
assert.sameValue(zone.getPreviousTransition(inst), null)
