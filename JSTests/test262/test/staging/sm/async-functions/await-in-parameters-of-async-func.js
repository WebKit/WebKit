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
var BUGNUMBER = 1478910;
var summary = 'JSMSG_AWAIT_IN_PARAMETER error for incomplete await expr in async function/generator parameter';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  let testAwaitInDefaultExprOfAsyncFunc = (code) => {
  	assertThrowsInstanceOf(() => eval(code), SyntaxError, "await expression can't be used in parameter");
  };

  let testNoException = (code) => {
  	assert.sameValue(completesNormally(code), true);
  };

  // https://www.ecma-international.org/ecma-262/9.0/

  // Async Generator Function Definitions : AsyncGeneratorDeclaration & AsyncGeneratorExpression
  // async function* f() {}
  // f = async function*() {}
  testAwaitInDefaultExprOfAsyncFunc("async function* f(a = await) {}");
  testAwaitInDefaultExprOfAsyncFunc("let f = async function*(a = await) {}");

  testAwaitInDefaultExprOfAsyncFunc("function f(a = async function*(a = await) {}) {}");
  testAwaitInDefaultExprOfAsyncFunc("function f() { a = async function*(a = await) {}; }");

  testAwaitInDefaultExprOfAsyncFunc("async function* f() { a = async function*(a = await) {}; }");
  testNoException("async function* f() { let a = function(a = await) {}; }");

  testNoException("async function* f(a = async function*() { await 1; }) {}");

  // Async Function Definitions : AsyncFunctionDeclaration & AsyncFunctionExpression
  // async function f() {}
  // f = async function() {}
  testAwaitInDefaultExprOfAsyncFunc("async function f(a = await) {}");
  testAwaitInDefaultExprOfAsyncFunc("let f = async function(a = await) {}");

  testAwaitInDefaultExprOfAsyncFunc("function f(a = async function(a = await) {}) {}");
  testAwaitInDefaultExprOfAsyncFunc("function f() { a = async function(a = await) {}; }");

  testAwaitInDefaultExprOfAsyncFunc("async function f() { a = async function(a = await) {}; }");
  testNoException("async function f() { let a = function(a = await) {}; }");

  testNoException("async function f(a = async function() { await 1; }) {}");

  // Async Arrow Function Definitions : AsyncArrowFunction
  // async () => {}
  testAwaitInDefaultExprOfAsyncFunc("async (a = await) => {}");

  testNoException("async (a = async () => { await 1; }) => {}");

}
