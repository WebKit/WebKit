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
var BUGNUMBER = 459085;
var summary = 'Do not assert with JIT: Should not move data from GPR to XMM';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  

  var m = new Number(3);
  function foo() { for (var i=0; i<20;i++) m.toString(); } 
  foo();


  assert.sameValue(expect, actual, summary);
}
