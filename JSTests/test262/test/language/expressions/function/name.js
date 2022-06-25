// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-function-definitions-runtime-semantics-evaluation
description: Assignment of function `name` attribute
info: |
    FunctionExpression : function ( FormalParameters ) { FunctionBody }

    1. Let scope be the LexicalEnvironment of the running execution context.
    2. Let closure be FunctionCreate(Normal, FormalParameters, FunctionBody,
       scope, "").
    ...
    5. Return closure.

    FunctionExpression : function BindingIdentifier ( FormalParameters ) { FunctionBody }

    1. Let scope be the running execution context's LexicalEnvironment.
    2. Let funcEnv be NewDeclarativeEnvironment(scope).
    3. Let envRec be funcEnv's EnvironmentRecord.
    4. Let name be StringValue of BindingIdentifier.
    5. Perform envRec.CreateImmutableBinding(name, false).
    6. Let closure be FunctionCreate(Normal, FormalParameters, FunctionBody,
       funcEnv, name).
    ...
    10. Return closure.
includes: [propertyHelper.js]
---*/

verifyProperty(function() {}, "name", {
  value: "", writable: false, enumerable: false, configurable: true
});

verifyProperty(function func() {}, "name", {
  value: "func", writable: false, enumerable: false, configurable: true
});
