/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var testArray = [1, 2, 3]
assert.sameValue(testArray['0' + '\0'], undefined);
assert.sameValue(testArray['1' + '\0' + 'aaaa'], undefined)
assert.sameValue(testArray['\0' + '2'], undefined);
assert.sameValue(testArray['\0' + ' 2'], undefined);

testArray['\0'] = 'hello';
testArray[' \0'] = 'world';
assert.sameValue(testArray['\0'], 'hello');
assert.sameValue(testArray[' \0'], 'world');

