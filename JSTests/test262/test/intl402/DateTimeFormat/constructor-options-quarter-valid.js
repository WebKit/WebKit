// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
    Checks handling of the options argument to the DateTimeFormat constructor.
info: |
   [[Quarter]]    `"quarter"`    `"narrow"`, `"short"`, `"long"`
    InitializeDateTimeFormat ( dateTimeFormat, locales, options )

    ...
features: [Intl.DateTimeFormat-quarter]
---*/


const validOptions = [
  [undefined, undefined],
  ["long", "long"],
  ["short", "short"],
  ["narrow", "narrow"],
  [{ toString() { return "narrow"; } }, "narrow"],
  [{ valueOf() { return "long"; }, toString: undefined }, "long"],
];
for (const [quarter, expected] of validOptions) {
  const dtf = new Intl.DateTimeFormat("en", { quarter });
  const options = dtf.resolvedOptions();
  assert.sameValue(options.quarter, expected);
  const propdesc = Object.getOwnPropertyDescriptor(options, "quarter");
  if (expected === undefined) {
    assert.sameValue(propdesc, undefined);
  } else {
    assert.sameValue(propdesc.value, expected);
  }
}
