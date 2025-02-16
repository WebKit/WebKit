/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 481516;
var summary = 'TM: pobj_ == obj2';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = '1111222';

  a = {x: 1};
  b = {__proto__: a};
  c = {__proto__: b};
  objs = [{__proto__: a}, {__proto__: a}, {__proto__: a}, b, {__proto__: a},
          {__proto__: a}];
  for (i = 0; i < 6; i++) {
    print(actual += ""+c.x);
    objs[i].x = 2;
  }
  print(actual += c.x);

  assert.sameValue(expect, actual, summary);
}
