// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.with()
features: [Temporal]
---*/

var dstStartDay = Temporal.PlainDateTime.from("2000-04-02T12:00:01").toZonedDateTime("America/Vancouver");
var dstEndDay = Temporal.PlainDateTime.from("2000-10-29T12:00:01").toZonedDateTime("America/Vancouver");
var oneThirty = {
  hour: 1,
  minute: 30
};
var twoThirty = {
  hour: 2,
  minute: 30
};

// Disambiguation options
var offset = "ignore";
// compatible, skipped wall time
assert.sameValue(`${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-04-02T03:30:01-07:00[America/Vancouver]");

// earlier, skipped wall time
assert.sameValue(`${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:01-08:00[America/Vancouver]");

// later, skipped wall time
assert.sameValue(`${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:01-07:00[America/Vancouver]");

// compatible, repeated wall time
assert.sameValue(`${ dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "compatible"
}) }`, "2000-10-29T01:30:01-07:00[America/Vancouver]");

// earlier, repeated wall time
assert.sameValue(`${ dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-10-29T01:30:01-07:00[America/Vancouver]");

// later, repeated wall time
assert.sameValue(`${ dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "later"
}) }`, "2000-10-29T01:30:01-08:00[America/Vancouver]");

// reject
assert.throws(RangeError, () => dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "reject"
}));
assert.throws(RangeError, () => dstEndDay.with(oneThirty, {
  offset,
  disambiguation: "reject"
}));

// compatible is the default
assert.sameValue(`${ dstStartDay.with(twoThirty, { offset }) }`, `${ dstStartDay.with(twoThirty, {
  offset,
  disambiguation: "compatible"
}) }`);
assert.sameValue(`${ dstEndDay.with(twoThirty, { offset }) }`, `${ dstEndDay.with(twoThirty, {
  offset,
  disambiguation: "compatible"
}) }`);

// Offset options
var bogus = {
  ...twoThirty,
  offset: "+23:59"
};
// use, with bogus offset, changes to the exact time with the offset
var preserveExact = dstStartDay.with(bogus, { offset: "use" });
assert.sameValue(`${ preserveExact }`, "2000-03-31T18:31:01-08:00[America/Vancouver]");
assert.sameValue(preserveExact.epochNanoseconds, Temporal.Instant.from("2000-04-02T02:30:01+23:59").epochNanoseconds);

// ignore, with bogus offset, defers to disambiguation option
var offset = "ignore";
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:01-08:00[America/Vancouver]");
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:01-07:00[America/Vancouver]");

// prefer, with bogus offset, defers to disambiguation option
var offset = "prefer";
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "earlier"
}) }`, "2000-04-02T01:30:01-08:00[America/Vancouver]");
assert.sameValue(`${ dstStartDay.with(bogus, {
  offset,
  disambiguation: "later"
}) }`, "2000-04-02T03:30:01-07:00[America/Vancouver]");

// reject, with bogus offset, throws
assert.throws(RangeError, () => dstStartDay.with({
  ...twoThirty,
  offset: "+23:59"
}, { offset: "reject" }));

var doubleTime = new Temporal.ZonedDateTime(972811801_000_000_000n, "America/Vancouver");
// use changes to the exact time with the offset
var preserveExact = doubleTime.with({ offset: "-07:00" }, { offset: "use" });
assert.sameValue(preserveExact.offset, "-07:00");
assert.sameValue(preserveExact.epochNanoseconds, Temporal.Instant.from("2000-10-29T01:30:01-07:00").epochNanoseconds);

// ignore defers to disambiguation option
var offset = "ignore";
assert.sameValue(doubleTime.with({ offset: "-07:00" }, {
  offset,
  disambiguation: "earlier"
}).offset, "-07:00");
assert.sameValue(doubleTime.with({ offset: "-07:00" }, {
  offset,
  disambiguation: "later"
}).offset, "-08:00");

// prefer adjusts offset of repeated clock time
assert.sameValue(doubleTime.with({ offset: "-07:00" }, { offset: "prefer" }).offset, "-07:00");

// reject adjusts offset of repeated clock time
assert.sameValue(doubleTime.with({ offset: "-07:00" }, { offset: "reject" }).offset, "-07:00");

// use does not cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "use" }).offset, "-08:00");

// ignore may cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "ignore" }).offset, "-07:00");

// prefer does not cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "prefer" }).offset, "-08:00");

// reject does not cause the offset to change when adjusting repeated clock time
assert.sameValue(doubleTime.with({ minute: 31 }, { offset: "reject" }).offset, "-08:00");

// prefer is the default
assert.sameValue(`${ dstStartDay.with(twoThirty) }`, `${ dstStartDay.with(twoThirty, { offset: "prefer" }) }`);
assert.sameValue(`${ dstEndDay.with(twoThirty) }`, `${ dstEndDay.with(twoThirty, { offset: "prefer" }) }`);
assert.sameValue(`${ doubleTime.with({ minute: 31 }) }`, `${ doubleTime.with({ minute: 31 }, { offset: "prefer" }) }`);
