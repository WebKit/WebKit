// This file was procedurally generated from the following sources:
// - src/class-fields/propname-constructor.case
// - src/class-fields/propname-error/cls-expr-computed-name.template
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


var x = "constructor";
var C = class {
  [x];
};

var c = new C();
assert.sameValue(c.hasOwnProperty("constructor"), true);

assert.sameValue(C.hasOwnProperty("constructor"), false);
