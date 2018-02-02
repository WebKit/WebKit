// This file was procedurally generated from the following sources:
// - src/class-fields/propname-constructor.case
// - src/class-fields/propname-error/cls-decl-variable-name.template
/*---
description: class fields forbid PropName 'constructor' (no early error -- PropName of ComputedPropertyName not forbidden value)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-public]
flags: [generated]
info: |
    Static Semantics: PropName
    ...
    ComputedPropertyName : [ AssignmentExpression ]
      Return empty.

    
    // This test file tests the following early error:
    Static Semantics: Early Errors

      ClassElement : FieldDefinition;
        It is a Syntax Error if PropName of FieldDefinition is "constructor".

---*/


var constructor = 'foo';
class C {
  [constructor];
}

var c = new C();

assert.sameValue(c.hasOwnProperty("foo"), true);
assert.sameValue(C.hasOwnProperty("foo"), false);
