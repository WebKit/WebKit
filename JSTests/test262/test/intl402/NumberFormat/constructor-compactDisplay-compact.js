// Copyright 2019 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializenumberformat
description: Checks handling of the compactDisplay option to the NumberFormat constructor.
info: |
    InitializeNumberFormat ( numberFormat, locales, options )

    19. Let compactDisplay be ? GetOption(options, "compactDisplay", "string", « "short", "long" », "short").
    20. If notation is "compact", then
        a. Set numberFormat.[[CompactDisplay]] to compactDisplay.

includes: [compareArray.js]
features: [Intl.NumberFormat-unified]
---*/

const values = [
  [undefined, "short"],
  ["short"],
  ["long"],
];

for (const [value, expected = value] of values) {
  const callOrder = [];
  const nf = new Intl.NumberFormat([], {
    get notation() {
      callOrder.push("notation");
      return "compact";
    },
    get compactDisplay() {
      callOrder.push("compactDisplay");
      return value;
    }
  });
  const resolvedOptions = nf.resolvedOptions();
  assert.sameValue("compactDisplay" in resolvedOptions, true);
  assert.sameValue(resolvedOptions.compactDisplay, expected);

  assert.compareArray(callOrder, [
    "notation",
    "compactDisplay",
  ]);
}
