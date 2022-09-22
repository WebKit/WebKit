// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.toInstant()
features: [Temporal]
---*/


// recent date
var zdt = Temporal.ZonedDateTime.from("2019-10-29T10:46:38.271986102+01:00[Europe/Amsterdam]");
assert.sameValue(`${ zdt.toInstant() }`, "2019-10-29T09:46:38.271986102Z");

// year ≤ 99
var zdt = Temporal.ZonedDateTime.from("0098-10-29T10:46:38.271986102+00:00[UTC]");
assert.sameValue(`${ zdt.toInstant() }`, "0098-10-29T10:46:38.271986102Z");
zdt = Temporal.ZonedDateTime.from("+000098-10-29T10:46:38.271986102+00:00[UTC]");
assert.sameValue(`${ zdt.toInstant() }`, "0098-10-29T10:46:38.271986102Z");

// year < 1
var zdt = Temporal.ZonedDateTime.from("0000-10-29T10:46:38.271986102+00:00[UTC]");
assert.sameValue(`${ zdt.toInstant() }`, "0000-10-29T10:46:38.271986102Z");
zdt = Temporal.ZonedDateTime.from("+000000-10-29T10:46:38.271986102+00:00[UTC]");
assert.sameValue(`${ zdt.toInstant() }`, "0000-10-29T10:46:38.271986102Z");
zdt = Temporal.ZonedDateTime.from("-001000-10-29T10:46:38.271986102+00:00[UTC]");
assert.sameValue(`${ zdt.toInstant() }`, "-001000-10-29T10:46:38.271986102Z");

// year 0 leap day
var zdt = Temporal.ZonedDateTime.from("0000-02-29T00:00-00:01:15[Europe/London]");
assert.sameValue(`${ zdt.toInstant() }`, "0000-02-29T00:01:15Z");
zdt = Temporal.ZonedDateTime.from("+000000-02-29T00:00-00:01:15[Europe/London]");
assert.sameValue(`${ zdt.toInstant() }`, "0000-02-29T00:01:15Z");
