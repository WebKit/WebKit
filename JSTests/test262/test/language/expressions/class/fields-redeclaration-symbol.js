// This file was procedurally generated from the following sources:
// - src/class-fields/redeclaration-symbol.case
// - src/class-fields/default/cls-expr.template
/*---
description: Redeclaration of public fields with the same name (field definitions in a class expression)
esid: prod-FieldDefinition
features: [class, class-fields-public]
flags: [generated]
includes: [propertyHelper.js, compareArray.js]
info: |
    2.13.2 Runtime Semantics: ClassDefinitionEvaluation

    ...
    30. Set the value of F's [[Fields]] internal slot to fieldRecords.
    ...

    2.14 [[Construct]] ( argumentsList, newTarget)

    ...
    8. If kind is "base", then
      ...
      b. Let result be InitializeInstanceFields(thisArgument, F).
    ...

    2.9 InitializeInstanceFields ( O, constructor )

    3. Let fieldRecords be the value of constructor's [[Fields]] internal slot.
    4. For each item fieldRecord in order from fieldRecords,
      a. If fieldRecord.[[static]] is false, then
        i. Perform ? DefineField(O, fieldRecord).

---*/
var x = [];
var y = Symbol();


var C = class {
  [y] = (x.push("a"), "old_value");
  [y] = (x.push("b"), "same_value");
  [y] = (x.push("c"), "same_value");
}

var c = new C();

assert.sameValue(Object.hasOwnProperty.call(C.prototype, y), false);
assert.sameValue(Object.hasOwnProperty.call(C, y), false);

verifyProperty(c, y, {
  value: "same_value",
  enumerable: true,
  writable: true,
  configurable: true
});

assert(compareArray(x, ["a", "b", "c"]));
