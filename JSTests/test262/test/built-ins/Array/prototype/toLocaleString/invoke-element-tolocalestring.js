// Copyright (C) 2022 Richard Gibson. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.tolocalestring
description: >
    The toLocaleString method of each non-undefined non-null element
    must be called with no arguments.
info: |
    Array.prototype.toLocaleString ( [ _reserved1_ [ , _reserved2_ ] ] )

    4. Let _R_ be the empty String.
    5. Let _k_ be 0.
    6. Repeat, while _k_ &lt; _len_,
      a. If _k_ &gt; 0, then
        i. Set _R_ to the string-concatenation of _R_ and _separator_.
      b. Let _nextElement_ be ? Get(_array_, ! ToString(_k_)).
      c. If _nextElement_ is not *undefined* or *null*, then
        i. Let _S_ be ? ToString(? Invoke(_nextElement_, *"toLocaleString"*)).
        ii. Set _R_ to the string-concatenation of _R_ and _S_.
      d. Increase _k_ by 1.
    7. Return _R_.
includes: [compareArray.js]
---*/

const unique = {
  toString() {
    return "<sentinel object>";
  }
};

const testCases = [
  { label: "no arguments", args: [] },
  { label: "undefined locale", args: [undefined] },
  { label: "string locale", args: ["ar"] },
  { label: "object locale", args: [unique] },
  { label: "undefined locale and options", args: [undefined, unique] },
  { label: "string locale and options", args: ["zh", unique] },
  { label: "object locale and options", args: [unique, unique] },
  { label: "extra arguments", args: [unique, unique, unique] },
];

for (const { label, args } of testCases) {
  assert.sameValue([undefined].toLocaleString(...args), "",
    `must skip undefined elements when provided ${label}`);
}
for (const { label, args, expectedArgs } of testCases) {
  assert.sameValue([null].toLocaleString(...args), "",
    `must skip null elements when provided ${label}`);
}

if (typeof Intl !== "object") {
  for (const { label, args } of testCases) {
    const spy = {
      toLocaleString(...receivedArgs) {
        assert.compareArray(receivedArgs, [],
          `must invoke element toLocaleString with no arguments when provided ${label}`);
        return "ok";
      }
    };
    assert.sameValue([spy].toLocaleString(...args), "ok");
  }
}
