// This file was procedurally generated from the following sources:
// - src/arguments/args-trailing-comma-spread-operator.case
// - src/arguments/default/async-gen-func-expr.template
/*---
description: A trailing comma should not increase the arguments.length, using spread args (async generator function expression)
esid: sec-asyncgenerator-definitions-evaluation
features: [async-iteration]
flags: [generated, async]
info: |
    AsyncGeneratorExpression : async [no LineTerminator here] function * ( FormalParameters ) {
        AsyncGeneratorBody }

        [...]
        3. Let closure be ! AsyncGeneratorFunctionCreate(Normal, FormalParameters,
           AsyncGeneratorBody, scope, strict).
        [...]


    Trailing comma in the arguments list

    Left-Hand-Side Expressions

    Arguments :
        ( )
        ( ArgumentList )
        ( ArgumentList , )

    ArgumentList :
        AssignmentExpression
        ... AssignmentExpression
        ArgumentList , AssignmentExpression
        ArgumentList , ... AssignmentExpression
---*/
var arr = [2, 3];



var callCount = 0;
// Stores a reference `ref` for case evaluation
var ref;
ref = async function*() {
  assert.sameValue(arguments.length, 4);
  assert.sameValue(arguments[0], 42);
  assert.sameValue(arguments[1], 1);
  assert.sameValue(arguments[2], 2);
  assert.sameValue(arguments[3], 3);
  callCount = callCount + 1;
};

ref(42, ...[1], ...arr,).next().then(() => {
    assert.sameValue(callCount, 1, 'generator function invoked exactly once');
}).then($DONE, $DONE);
