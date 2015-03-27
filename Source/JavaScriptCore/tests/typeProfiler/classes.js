load("./driver/driver.js");

function wrapper() {

class Base { constructor() { this._base = true; } };
class Derived extends Base { constructor() { super(); this._derived = true; } };

var baseInstance = new Base;
var derivedInstance = new Derived;

}
wrapper();

// ====== End test cases ======

var types = findTypeForExpression(wrapper, "baseInstance = new Base");
assert(types.globalTypeSet.displayTypeName === "Base", "variable 'baseInstance' should have displayTypeName 'Base'");
assert(types.instructionTypeSet.displayTypeName === "Base", "variable 'baseInstance' should have displayTypeName 'Base'");
assert(types.instructionTypeSet.structures.length === 1, "Variable 'baseInstance' should have one structure");
assert(types.instructionTypeSet.structures[0].fields.length === 1, "variable 'baseInstance' should have one field: _base");
assert(types.instructionTypeSet.structures[0].fields.indexOf("_base") !== -1, "variable 'baseInstance' should have field '_base'");

types = findTypeForExpression(wrapper, "derivedInstance = new Derived");
assert(types.globalTypeSet.displayTypeName === "Derived", "variable 'derivedInstance' should have displayTypeName 'Derived'");
assert(types.instructionTypeSet.displayTypeName === "Derived", "variable 'derivedInstance' should have displayTypeName 'Derived'");
assert(types.instructionTypeSet.structures.length === 1, "Variable 'derivedInstance' should have one structure");
assert(types.instructionTypeSet.structures[0].fields.length === 2, "variable 'derivedInstance' should have one field: _base,_derived");
assert(types.instructionTypeSet.structures[0].fields.indexOf("_base") !== -1, "variable 'derivedInstance' should have field '_base'");
assert(types.instructionTypeSet.structures[0].fields.indexOf("_derived") !== -1, "variable 'derivedInstance' should have field '_derived'");
