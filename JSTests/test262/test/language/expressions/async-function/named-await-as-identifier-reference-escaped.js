// This file was procedurally generated from the following sources:
// - src/async-functions/await-as-identifier-reference-escaped.case
// - src/async-functions/syntax/async-expression-named.template
/*---
description: await is a reserved keyword within generator function bodies and may not be used as an identifier reference. (Named async function expression)
esid: prod-AsyncFunctionExpression
features: [async-functions]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Async Function Definitions

    AsyncFunctionExpression :
      async [no LineTerminator here] function BindingIdentifier ( FormalParameters ) { AsyncFunctionBody }


    IdentifierReference : Identifier

    It is a Syntax Error if this production has a [Await] parameter and
    StringValue of Identifier is "await".

---*/
$DONOTEVALUATE();


var asyncFn = async function asyncFn() {
  void \u0061wait;
};
