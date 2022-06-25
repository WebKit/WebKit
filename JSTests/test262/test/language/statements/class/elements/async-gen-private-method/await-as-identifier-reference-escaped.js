// This file was procedurally generated from the following sources:
// - src/async-generators/await-as-identifier-reference-escaped.case
// - src/async-generators/syntax/async-class-decl-private-method.template
/*---
description: await is a reserved keyword within generator function bodies and may not be used as an identifier reference. (Async Generator private method as a ClassDeclaration element)
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


    IdentifierReference : Identifier

    It is a Syntax Error if this production has a [Await] parameter and
    StringValue of Identifier is "await".

---*/
$DONOTEVALUATE();


class C { async *#gen() {
    void \u0061wait;
}}
