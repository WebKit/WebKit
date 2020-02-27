// Copyright 2019 Google Inc, Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-partitiondatetimepattern
description: >
  Checks the output of 'relatedYear' and 'yearName' type, and
  the choice of pattern based on calendar.
locale: [zh-u-ca-chinese]
---*/

function verifyFormatParts(actual, expected, message) {
  assert.sameValue(Array.isArray(expected), true, `${message}: expected is Array`);
  assert.sameValue(Array.isArray(actual), true, `${message}: actual is Array`);
  assert.sameValue(actual.length, expected.length, `${message}: length`);

  for (let i = 0; i < actual.length; ++i) {
    assert.sameValue(actual[i].type, expected[i].type, `${message}: parts[${i}].type`);
    assert.sameValue(actual[i].value, expected[i].value, `${message}: parts[${i}].value`);
  }
}

const df = new Intl.DateTimeFormat("zh-u-ca-chinese", {year: "numeric"});
const date = new Date(2019, 5, 1);
verifyFormatParts(df.formatToParts(date), [
  {type: "relatedYear", value: "2019"},
  {type: "yearName", value: "己亥"},
  {type: "literal", value: "年"},
]);
