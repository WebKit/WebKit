// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-function-calls-runtime-semantics-evaluation
description: >
  Check TypeError is thrown from correct realm with tco-call to class constructor from [[Call]] invocation.
info: |
  12.3.4.3 Runtime Semantics: EvaluateDirectCall( func, thisValue, arguments, tailPosition )
    ...
    4. If tailPosition is true, perform PrepareForTailCall().
    5. Let result be Call(func, thisValue, argList).
    6. Assert: If tailPosition is true, the above call will not return here, but instead evaluation will continue as if the following return has already occurred.
    7. Assert: If result is not an abrupt completion, then Type(result) is an ECMAScript language type.
    8. Return result.

  9.2.1 [[Call]] ( thisArgument, argumentsList)
    ...
    2. If F.[[FunctionKind]] is "classConstructor", throw a TypeError exception.
    3. Let callerContext be the running execution context.
    4. Let calleeContext be PrepareForOrdinaryCall(F, undefined).
    5. Assert: calleeContext is now the running execution context.
    ...

features: [tail-call-optimization, class]
---*/

// - The class constructor call is in a valid tail-call position, which means PrepareForTailCall is performed.
// - The function call returns from `otherRealm` and proceeds the tail-call in this realm.
// - Calling the class constructor throws a TypeError from the current realm, that means this realm and not `otherRealm`.
var code = "'use strict'; (function() { return (class {})(); });";

var otherRealm = $262.createRealm();
var tco = otherRealm.evalScript(code);

assert.throws(TypeError, function() {
  tco();
});
