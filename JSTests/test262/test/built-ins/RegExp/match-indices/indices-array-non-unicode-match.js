// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Basic matching cases with non-unicode matches.
includes: [compareArray.js, propertyHelper.js, deepEqual.js]
esid: sec-regexpbuiltinexec
features: [regexp-match-indices]
info: |
  Runtime Semantics: RegExpBuiltinExec ( R, S )
    ...
    4. Let _lastIndex_ be ? ToLength(? Get(_R_, `"lastIndex")).
    ...
    25. Let _indices_ be a new empty List.
    26. Let _match_ be the Match { [[StartIndex]]: _lastIndex_, [[EndIndex]]: _e_ }.
    27. Add _match_ as the last element of _indices_.
    ...
    33. For each integer _i_ such that _i_ > 0 and _i_ <= _n_, in ascending order, do
      ...
      f. Else,
        i. Let _captureStart_ be _captureI_'s _startIndex_.
        ii. Let _captureEnd_ be _captureI_'s _endIndex_.
        ...
        iv. Let _capture_ be the Match  { [[StartIndex]]: _captureStart_, [[EndIndex]]: _captureEnd_ }.
        v. Append _capture_ to _indices_.
        ...
    34. Let _indicesArray_ be MakeIndicesArray( _S_, _indices_, _groupNames_).
---*/

assert.deepEqual([[1, 2], [1, 2]], "bab".match(/(a)/).indices);
assert.deepEqual([[0, 3], [1, 2]], "bab".match(/.(a)./).indices);
assert.deepEqual([[0, 3], [1, 2], [2, 3]], "bab".match(/.(a)(.)/).indices);
assert.deepEqual([[0, 3], [1, 3]], "bab".match(/.(\w\w)/).indices);
assert.deepEqual([[0, 3], [0, 3]], "bab".match(/(\w\w\w)/).indices);
assert.deepEqual([[0, 3], [0, 2], [2, 3]], "bab".match(/(\w\w)(\w)/).indices);
assert.deepEqual([[0, 2], [0, 2], undefined], "bab".match(/(\w\w)(\W)?/).indices);

let groups = /(?<a>.)(?<b>.)(?<c>.)\k<c>\k<b>\k<a>/.exec("abccba").indices.groups;
assert.compareArray([0, 1], groups.a);
assert.compareArray([1, 2], groups.b);
assert.compareArray([2, 3], groups.c);
verifyProperty(groups, "a", {
    enumerable: true,
    writable: true,
    configurable: true
});
verifyProperty(groups, "b", {
    enumerable: true,
    writable: true,
    configurable: true
});
verifyProperty(groups, "c", {
    enumerable: true,
    writable: true,
    configurable: true
});

// "𝐁" is U+1d401 MATHEMATICAL BOLD CAPITAL B
// - Also representable as the code point "\u{1d401}"
// - Also representable as the surrogate pair "\uD835\uDC01"

// Verify assumptions:
assert.sameValue("𝐁".length, 2, 'The length of "𝐁" is 2');
assert.sameValue("\u{1d401}".length, 2, 'The length of "\\u{1d401}" is 2');
assert.sameValue("\uD835\uDC01".length, 2, 'The length of "\\uD835\\uDC01" is 2');
assert.sameValue("𝐁".match(/./)[0].length, 1, 'The length of a single code unit match against "𝐁" is 1 (without /u flag)');
assert.sameValue("\u{1d401}".match(/./)[0].length, 1, 'The length of a single code unit match against "\\u{1d401}" is 1 (without /u flag)');
assert.sameValue("\uD835\uDC01".match(/./)[0].length, 1, 'The length of a single code unit match against "\\ud835\\udc01" is 1 (without /u flag)');

assert.compareArray([0, 1], "𝐁".match(/./).indices[0], 'Indices for non-unicode match against "𝐁" (without /u flag)');
assert.compareArray([0, 1], "\u{1d401}".match(/./).indices[0], 'Indices for non-unicode match against "\\u{1d401}" (without /u flag)');
assert.compareArray([0, 1], "\uD835\uDC01".match(/./).indices[0], 'Indices for non-unicode match against "\\ud835\\udc01" (without /u flag)');
assert.compareArray([0, 1], "𝐁".match(/(?<a>.)/).indices.groups.a, 'Indices for non-unicode match against "𝐁" in groups.a (without /u flag)');
assert.compareArray([0, 1], "\u{1d401}".match(/(?<a>.)/).indices.groups.a, 'Indices for non-unicode match against "\\u{1d401}" in groups.a (without /u flag)');
assert.compareArray([0, 1], "\uD835\uDC01".match(/(?<a>.)/).indices.groups.a, 'Indices for non-unicode match against "\\ud835\\udc01" in groups.a (without /u flag)');
