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
//-----------------------------------------------------------------------------
var BUGNUMBER = 469625;
var summary = 'Do not assert: script->objectsOffset != 0';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function f(x) {
    var [a, b, [c0, c1]] = [x, x, x];
  }
  assertThrowsInstanceOfWithMessageCheck(
    () => f(null),
    TypeError,
    message => /.*\[\.\.\.\]\[Symbol.iterator\]\(\)\.next\(\)\.value is null/.exec(message) !== null
  );
}

