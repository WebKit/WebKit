// Copyright 2019 Google Inc, Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-partitiondatetimepattern
description: >
  Checks the output of 'relatedYear' and 'yearName' type, and
  the choice of pattern based on calendar.
locale: [zh-u-ca-chinese]
---*/

const df = new Intl.DateTimeFormat("zh-u-ca-chinese", {year: "numeric"});
const date = new Date(2019, 5, 1);
assert.sameValue(df.format(date), "2019己亥年");
