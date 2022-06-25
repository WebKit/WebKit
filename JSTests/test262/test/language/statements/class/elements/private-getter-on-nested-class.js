// This file was procedurally generated from the following sources:
// - src/class-elements/private-getter-on-nested-class.case
// - src/class-elements/default/cls-decl.template
/*---
description: PrivateName of private getter is available on inner classes (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [class-methods-private, class-fields-public, class]
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
  get #m() { return 'test262'; }

  B = class {
    method(o) {
      return o.#m;
    }
  }
}

let c = new C();
let innerB = new c.B();
assert.sameValue(innerB.method(c), 'test262');
