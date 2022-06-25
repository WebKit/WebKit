// Copyright 2019 Mozilla Corporation, Igalia S.L. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.resolvedOptions
description: >
  Intl.DateTimeFormat.prototype.resolvedOptions properly
  reflect hourCycle settings when using dateStyle.
features: [Intl.DateTimeFormat-datetimestyle]
---*/

const hcValues = ["h11", "h12", "h23", "h24"];
const hour12Values = ["h11", "h12"];

for (const dateStyle of ["full", "long", "medium", "short"]) {
  assert.sameValue(new Intl.DateTimeFormat([], { dateStyle }).resolvedOptions().dateStyle,
                   dateStyle,
                   `Should support dateStyle=${dateStyle}`);

  /* Values passed via unicode extension key work */

  for (const hcValue of hcValues) {
    const resolvedOptions = new Intl.DateTimeFormat(`de-u-hc-${hcValue}`, {
      dateStyle,
    }).resolvedOptions();

    assert.sameValue(resolvedOptions.hourCycle, undefined);
    assert.sameValue(resolvedOptions.hour12, undefined);
  }

  /* Values passed via options work */

  for (const hcValue of hcValues) {
    const resolvedOptions = new Intl.DateTimeFormat("en-US", {
      dateStyle,
      hourCycle: hcValue
    }).resolvedOptions();

    assert.sameValue(resolvedOptions.hourCycle, undefined);
    assert.sameValue(resolvedOptions.hour12, undefined);
  }

  let resolvedOptions = new Intl.DateTimeFormat("en-US-u-hc-h12", {
    dateStyle,
    hourCycle: "h23"
  }).resolvedOptions();

  assert.sameValue(resolvedOptions.hourCycle, undefined);
  assert.sameValue(resolvedOptions.hour12, undefined);

  resolvedOptions = new Intl.DateTimeFormat("fr", {
    dateStyle,
    hour12: true,
    hourCycle: "h23"
  }).resolvedOptions();

  assert.sameValue(resolvedOptions.hourCycle, undefined);
  assert.sameValue(resolvedOptions.hour12, undefined);

  resolvedOptions = new Intl.DateTimeFormat("fr-u-hc-h24", {
    dateStyle,
    hour12: true,
  }).resolvedOptions();

  assert.sameValue(resolvedOptions.hourCycle, undefined);
  assert.sameValue(resolvedOptions.hour12, undefined);
}
