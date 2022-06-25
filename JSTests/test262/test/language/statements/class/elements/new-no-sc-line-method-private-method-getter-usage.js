// This file was procedurally generated from the following sources:
// - src/class-elements/private-method-getter-usage.case
// - src/class-elements/productions/cls-decl-new-no-sc-line-method.template
/*---
description: PrivateName CallExpression usage (Accesor get method) (field definitions followed by a method in a new line without a semicolon)
esid: prod-FieldDefinition
features: [class-methods-private, class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js]
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
  get #m() { return 'test262'; }
  m() { return 42; }
  method() {
    return this.#m;
  }
}

var c = new C();

assert.sameValue(c.m(), 42);
assert.sameValue(c.m, C.prototype.m);
assert.sameValue(Object.hasOwnProperty.call(c, "m"), false);

verifyProperty(C.prototype, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
});

assert.sameValue(c.method(), 'test262');
