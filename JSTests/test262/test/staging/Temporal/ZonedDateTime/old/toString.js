// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toString()
features: [Temporal]
---*/

var zdt1 = Temporal.ZonedDateTime.from("1976-11-18T15:23+00:00[UTC]");

// shows offset if offset = auto
assert.sameValue(zdt1.toString({ offset: "auto" }), "1976-11-18T15:23:00+00:00[UTC]");

// omits offset if offset = never
assert.sameValue(zdt1.toString({ offset: "never" }), "1976-11-18T15:23:00[UTC]");
