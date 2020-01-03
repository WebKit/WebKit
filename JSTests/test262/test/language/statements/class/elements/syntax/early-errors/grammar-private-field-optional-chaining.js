// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-private-field-optional-chaining.case
// - src/class-elements/syntax/invalid/cls-decl-elements-invalid-syntax.template
/*---
description: PrivateName after '?.' is not valid syntax (class declaration)
esid: prod-ClassElement
features: [class-fields-private, optional-chaining, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Updated Productions

    MemberExpression[Yield]:
      MemberExpression[?Yield].PrivateName

---*/


$DONOTEVALUATE();

class C {
  #m = 'test262';

  access(obj) {
    return obj?.#m;
  }
}
