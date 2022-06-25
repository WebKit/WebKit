// This file was procedurally generated from the following sources:
// - src/async-generators/yield-as-label-identifier-escaped.case
// - src/async-generators/syntax/async-class-expr-private-method.template
/*---
description: yield is a reserved keyword within generator function bodies and may not be used as a label identifier. (Async generator private method as a ClassExpression element)
esid: prod-AsyncGeneratorMethod
features: [async-iteration, class-methods-private]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    ClassElement :
      PrivateMethodDefinition

    MethodDefinition :
      AsyncGeneratorMethod

    Async Generator Function Definitions

    AsyncGeneratorMethod :
      async [no LineTerminator here] * # PropertyName ( UniqueFormalParameters ) { AsyncGeneratorBody }


    LabelIdentifier : Identifier

    It is a Syntax Error if this production has a [Yield] parameter and
    StringValue of Identifier is "yield".

---*/
$DONOTEVALUATE();


var C = class { async *#gen() {
    yi\u0065ld: ;
}};
