// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - IsHTMLDDA
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1566141;
var summary = "Implement the Nullish Coalescing operator (??) proposal";

print(BUGNUMBER + ": " + summary);

// These tests are originally from webkit.
// webkit specifics have been removed and a test for `document.all` has
// been added.
function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(script) {
  eval(script);
}

function shouldThrowSyntaxError(script) {
  let error;
  try {
    eval(script);
  } catch (e) {
    error = e;
  }

  if (!(error instanceof SyntaxError))
    throw new Error('Expected SyntaxError!');
}

function testBasicCases() {
  shouldBe(undefined ?? 3, 3);
  shouldBe(null ?? 3, 3);
  shouldBe(true ?? 3, true);
  shouldBe(false ?? 3, false);
  shouldBe(0 ?? 3, 0);
  shouldBe(1 ?? 3, 1);
  shouldBe('' ?? 3, '');
  shouldBe('hi' ?? 3, 'hi');
  shouldBe(({} ?? 3) instanceof Object, true);
  shouldBe(({ x: 'hi' } ?? 3).x, 'hi');
  shouldBe(([] ?? 3) instanceof Array, true);
  shouldBe((['hi'] ?? 3)[0], 'hi');
  // test document.all, which has odd behavior
  shouldBe(typeof(createIsHTMLDDA() ?? 3), "undefined");
}

for (let i = 0; i < 1e5; i++)
  testBasicCases();

shouldBe(1 | null ?? 3, 1);
shouldBe(1 ^ null ?? 3, 1);
shouldBe(1 & null ?? 3, 0);
shouldBe(3 == null ?? 3, false);
shouldBe(3 != null ?? 3, true);
shouldBe(3 === null ?? 3, false);
shouldBe(3 !== null ?? 3, true);
shouldBe(1 < null ?? 3, false);
shouldBe(1 > null ?? 3, true);
shouldBe(1 <= null ?? 3, false);
shouldBe(1 >= null ?? 3, true);
shouldBe(1 << null ?? 3, 1);
shouldBe(1 >> null ?? 3, 1);
shouldBe(1 >>> null ?? 3, 1);
shouldBe(1 + null ?? 3, 1);
shouldBe(1 - null ?? 3, 1);
shouldBe(1 * null ?? 3, 0);
shouldBe(1 / null ?? 3, Infinity);
shouldBe(isNaN(1 % null ?? 3), true);
shouldBe(1 ** null ?? 3, 1);
shouldBe((void 0) ?? 3, 3);

const obj = {
      count: 0,
          get x() { this.count++; return 'x'; }
};
false ?? obj.x;
shouldBe(obj.count, 0);
null ?? obj.x;
shouldBe(obj.count, 1);
obj.x ?? obj.x;
shouldBe(obj.count, 2);

shouldThrowSyntaxError('0 || 1 ?? 2');
shouldThrowSyntaxError('0 && 1 ?? 2');
shouldThrowSyntaxError('0 ?? 1 || 2');
shouldThrowSyntaxError('0 ?? 1 && 2');
shouldNotThrow('(0 || 1) ?? 2');
shouldNotThrow('0 || (1 ?? 2)');
shouldNotThrow('(0 && 1) ?? 2');
shouldNotThrow('0 && (1 ?? 2)');
shouldNotThrow('(0 ?? 1) || 2');
shouldNotThrow('0 ?? (1 || 2)');
shouldNotThrow('(0 ?? 1) && 2');
shouldNotThrow('0 ?? (1 && 2)');

shouldNotThrow('0 || 1 && 2 | 3 ^ 4 & 5 == 6 != 7 === 8 !== 9 < 0 > 1 <= 2 >= 3 << 4 >> 5 >>> 6 + 7 - 8 * 9 / 0 % 1 ** 2');
shouldThrowSyntaxError('0 || 1 && 2 | 3 ^ 4 & 5 == 6 != 7 === 8 !== 9 < 0 > 1 <= 2 >= 3 << 4 >> 5 >>> 6 + 7 - 8 * 9 / 0 % 1 ** 2 ?? 3');
shouldThrowSyntaxError('3 ?? 2 ** 1 % 0 / 9 * 8 - 7 + 6 >>> 5 >> 4 << 3 >= 2 <= 1 > 0 < 9 !== 8 === 7 != 6 == 5 & 4 ^ 3 | 2 && 1 || 0');

shouldBe(null?.x ?? 3, 3);
shouldBe(({})?.x ?? 3, 3);
shouldBe(({ x: 0 })?.x ?? 3, 0);
shouldBe(null?.() ?? 3, 3);
shouldBe((() => 0)?.() ?? 3, 0);
shouldBe(({ x: 0 })?.[null?.a ?? 'x'] ?? 3, 0);
shouldBe((() => 0)?.(null?.a ?? 'x') ?? 3, 0);

print("Tests complete");

