// Copyright 2012 Norbert Lindenberg. All rights reserved.
// Copyright 2012 Mozilla Corporation. All rights reserved.
// Copyright 2013 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
es5id: 15.5.4.9_CE
description: >
    Tests that String.prototype.localeCompare  returns 0 when
    comparing Strings that are considered canonically equivalent  by
    the Unicode standard.
author: Norbert Lindenberg
---*/

// pairs with characters not in Unicode 3.0 are commented out
var pairs = [
  // example from Unicode 5.0, section 3.7, definition D70
  ["o\u0308", "ö"],
  // examples from Unicode 5.0, chapter 3.11
  ["ä\u0323", "a\u0323\u0308"],
  ["a\u0308\u0323", "a\u0323\u0308"],
  ["ạ\u0308", "a\u0323\u0308"],
  ["ä\u0306", "a\u0308\u0306"],
  ["ă\u0308", "a\u0306\u0308"],
  // example from Unicode 5.0, chapter 3.12
  ["\u1111\u1171\u11B6", "퓛"],
  // examples from UTS 10, Unicode Collation Algorithm
  ["Å", "Å"],
  ["Å", "A\u030A"],
  ["x\u031B\u0323", "x\u0323\u031B"],
  ["ự", "ụ\u031B"],
  ["ự", "u\u031B\u0323"],
  ["ự", "ư\u0323"],
  ["ự", "u\u0323\u031B"],
  // examples from UAX 15, Unicode Normalization Forms
  ["Ç", "C\u0327"],
  ["q\u0307\u0323", "q\u0323\u0307"],
  ["가", "\u1100\u1161"],
  ["Å", "A\u030A"],
  ["Ω", "Ω"],
  ["Å", "A\u030A"],
  ["ô", "o\u0302"],
  ["ṩ", "s\u0323\u0307"],
  ["ḋ\u0323", "d\u0323\u0307"],
  ["ḋ\u0323", "ḍ\u0307"],
  ["q\u0307\u0323", "q\u0323\u0307"],
  // examples involving supplementary characters from UCD NormalizationTest.txt
  //  ["\uD834\uDD5E", "\uD834\uDD57\uD834\uDD65"],
  //  ["\uD87E\uDC2B", "北"]
];

// Detect whether we are using locale-sensitive comparisons or a bitwise comparison
if ("a".localeCompare("Z") < 0) {
  // We are using locale-sensitive comparison, so all pairs should be canonically equivalent
  var i;
  for (i = 0; i < pairs.length; i++) {
    var pair = pairs[i];
    if (pair[0].localeCompare(pair[1]) !== 0) {
      throw new Test262Error("String.prototype.localeCompare considers " + pair[0] + " (" + toU(pair[0]) +
        ") ≠ " + pair[1] + " (" + toU(pair[1]) + ").");
    }
  }
} else {
  // We are using bitwise comparison, so all pairs should not be equivalent
  var i;
  for (i = 0; i < pairs.length; i++) {
    var pair = pairs[i];
    if (pair[0].localeCompare(pair[1]) === 0) {
      throw new Test262Error("String.prototype.localeCompare considers " + pair[0] + " (" + toU(pair[0]) +
        ") = " + pair[1] + " (" + toU(pair[1]) + ").");
    }
  }
}

function toU(s) {
  var result = "";
  var escape = "\\u0000";
  var i;
  for (i = 0; i < s.length; i++) {
    var hex = s.charCodeAt(i).toString(16);
    result += escape.substring(0, escape.length - hex.length) + hex;
  }
  return result;
}
