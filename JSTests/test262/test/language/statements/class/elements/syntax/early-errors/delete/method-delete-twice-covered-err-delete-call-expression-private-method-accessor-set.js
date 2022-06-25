// This file was procedurally generated from the following sources:
// - src/class-elements/err-delete-call-expression-private-method-accessor-set.case
// - src/class-elements/delete-error/cls-decl-method-delete-twice-covered.template
/*---
description: It's a SyntaxError if delete operator is applied to CallExpression.PrivateName accessor set method (in method, recursively covered)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-private, class-methods-private, class-fields-public]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    This file must never use the onlyStrict flag as the strict mode should always
    be observed inside class bodies.

    Static Semantics: Early Errors

      UnaryExpression : delete UnaryExpression

      It is a Syntax Error if the UnaryExpression is contained in strict mode
      code and the derived UnaryExpression is
      PrimaryExpression : IdentifierReference ,
      MemberExpression : MemberExpression.PrivateName , or
      CallExpression : CallExpression.PrivateName .

      It is a Syntax Error if the derived UnaryExpression is
      PrimaryExpression : CoverParenthesizedExpressionAndArrowParameterList and
      CoverParenthesizedExpressionAndArrowParameterList ultimately derives a
      phrase that, if used in place of UnaryExpression, would produce a
      Syntax Error according to these rules. This rule is recursively applied.

---*/


$DONOTEVALUATE();

class C {
  #x;

  x() {
    var g = this.f;
    delete ((g().#m));
  }

  f() {
  return this;
  }
}
