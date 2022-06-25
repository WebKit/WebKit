// This file was procedurally generated from the following sources:
// - src/class-elements/private-field-access-on-inner-function.case
// - src/class-elements/default/cls-decl.template
/*---
description: PrivateName of private field is visible on inner function of class scope (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [class-fields-private, class]
flags: [generated]
info: |
    Updated Productions

    CallExpression[Yield, Await]:
      CoverCallExpressionAndAsyncArrowHead[?Yield, ?Await]
      SuperCall[?Yield, ?Await]
      CallExpression[?Yield, ?Await]Arguments[?Yield, ?Await]
      CallExpression[?Yield, ?Await][Expression[+In, ?Yield, ?Await]]
      CallExpression[?Yield, ?Await].IdentifierName
      CallExpression[?Yield, ?Await]TemplateLiteral[?Yield, ?Await]
      CallExpression[?Yield, ?Await].PrivateName

---*/


class C {
  #f = 'Test262';

  method() {
    let self = this;
    function innerFunction() {
      return self.#f;
    }

    return innerFunction();
  }
}

let c = new C();
assert.sameValue(c.method(), 'Test262');
let o = {};
assert.throws(TypeError, function() {
  c.method.call(o);
}, 'accessed private field from an ordinary object');
