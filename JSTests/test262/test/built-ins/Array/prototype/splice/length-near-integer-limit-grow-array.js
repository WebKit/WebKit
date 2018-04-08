// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.splice
description: >
  A value is inserted in an array-like object whose length property is near the integer limit.
info: |
  ...
  16. Else if itemCount > actualDeleteCount, then
    a. Let k be (len - actualDeleteCount).
    b. Repeat, while k > actualStart
        i. Let from be ! ToString(k + actualDeleteCount - 1).
       ii. Let to be ! ToString(k + itemCount - 1).
      iii. Let fromPresent be ? HasProperty(O, from).
       iv. If fromPresent is true, then
          1. Let fromValue be ? Get(O, from).
          2. Perform ? Set(O, to, fromValue, true).
        v. Else fromPresent is false,
          1. Perform ? DeletePropertyOrThrow(O, to).
       vi. Decrease k by 1.
  ...
includes: [compareArray.js]
---*/

var arrayLike = {
  "9007199254740985": "9007199254740985",
  "9007199254740986": "9007199254740986",
  "9007199254740987": "9007199254740987",
  /* "9007199254740988": hole */
  "9007199254740989": "9007199254740989",
  /* "9007199254740990": empty */
  "9007199254740991": "9007199254740991",
  length: 2 ** 53 - 2,
};

var result = Array.prototype.splice.call(arrayLike, 9007199254740986, 0, "new-value");

assert.compareArray(result, [], "No elements are removed");

assert.sameValue(arrayLike.length, 2 ** 53 - 1, "New length is 2**53 - 1");

assert.sameValue(arrayLike["9007199254740985"], "9007199254740985",
  "arrayLike['9007199254740985'] is unchanged");

assert.sameValue(arrayLike["9007199254740986"], "new-value",
  "arrayLike['9007199254740986'] contains the inserted value");

assert.sameValue(arrayLike["9007199254740987"], "9007199254740986",
  "arrayLike['9007199254740986'] is moved to arrayLike['9007199254740987']");

assert.sameValue(arrayLike["9007199254740988"], "9007199254740987",
  "arrayLike['9007199254740987'] is moved to arrayLike['9007199254740988']");

assert.sameValue("9007199254740989" in arrayLike, false,
  "arrayLike['9007199254740989'] is removed");

assert.sameValue(arrayLike["9007199254740990"], "9007199254740989",
  "arrayLike['9007199254740989'] is moved to arrayLike['9007199254740990']");

assert.sameValue(arrayLike["9007199254740991"], "9007199254740991",
  "arrayLike['9007199254740991'] is unchanged");
