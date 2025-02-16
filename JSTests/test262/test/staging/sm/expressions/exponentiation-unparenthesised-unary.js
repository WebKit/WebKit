// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
const AsyncFunction = async function(){}.constructor;

function assertNoError(f, msg) {
  try {
    f();
  } catch (e) {
    assert.sameValue(true, false, `${msg}: ${e}`);
  }
}

function assertSyntaxError(code) {
  assertThrowsInstanceOf(function () { Function(code); }, SyntaxError, "Function:" + code);
  assertThrowsInstanceOf(function () { AsyncFunction(code); }, SyntaxError, "AsyncFunction:" + code);
}

function assertNoSyntaxError(code) {
  assertNoError(function () { Function(code); }, "Function:" + code);
  assertNoError(function () { AsyncFunction(code); }, "AsyncFunction:" + code);
}

function assertNoSyntaxErrorAsyncContext(code) {
  assertNoError(function () { AsyncFunction(code); }, "AsyncFunction:" + code);
}

const invalidTestCases = [
  // UnaryExpression : delete UnaryExpression
  //
  // Test all possible `delete` expression kinds.
  "delete a ** 0",
  "delete a.prop ** 0",
  "delete a[0] ** 0",
  "delete a?.prop ** 0",
  "delete 0 ** 0",

  // UnaryExpression : void UnaryExpression
  "void a ** 0",

  // UnaryExpression : typeof UnaryExpression
  //
  // Test all possible `typeof` expression kinds.
  "typeof a ** 0",
  "typeof 0 ** 0",

  // UnaryExpression : + UnaryExpression
  "+a ** 0",

  // UnaryExpression : - UnaryExpression
  "-a ** 0",

  // UnaryExpression : ~ UnaryExpression
  "~a ** 0",

  // UnaryExpression : ! UnaryExpression
  "!a ** 0",

  // UnaryExpression : AwaitExpression
  "await a ** 0",
];

for (let source of invalidTestCases) {
  assertSyntaxError(source);
}

const validTestCases = [
  // UnaryExpression : delete UnaryExpression
  "(delete a) ** 0",
  "(delete a.prop) ** 0",
  "(delete a[0]) ** 0",
  "(delete a?.prop) ** 0",
  "(delete 0) ** 0",

  // UnaryExpression : void UnaryExpression
  "(void a) ** 0",

  // UnaryExpression : typeof UnaryExpression
  "(typeof a) ** 0",
  "(typeof 0) ** 0",

  // UnaryExpression : + UnaryExpression
  "(+a) ** 0",

  // UnaryExpression : - UnaryExpression
  "(-a) ** 0",

  // UnaryExpression : ~ UnaryExpression
  "(~a) ** 0",

  // UnaryExpression : ! UnaryExpression
  "(!a) ** 0",
];

for (let source of validTestCases) {
  assertNoSyntaxError(source);
}

const validTestCasesAsync = [
  // UnaryExpression : AwaitExpression
  "(await a) ** 0",
];

for (let source of validTestCasesAsync) {
  assertNoSyntaxErrorAsyncContext(source);
}

