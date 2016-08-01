load("./driver/driver.js");

var func;
function wrapper() {

func = function(arg){};

}
wrapper();

// ====== End test cases ======

var obj = {x:20, y:50};

func(obj);
var types = findTypeForExpression(wrapper, "arg"); 
assert(types.instructionTypeSet.structures.length === 1, "arg should have one structure");
assert(types.instructionTypeSet.structures[0].fields.length === 2, "arg should have two fields");
assert(types.instructionTypeSet.structures[0].fields.indexOf("x") !== -1, "arg should have field: 'x'");
assert(types.instructionTypeSet.structures[0].fields.indexOf("y") !== -1, "arg should have field: 'y'");
assert(types.instructionTypeSet.structures[0].optionalFields.length === 0, "arg should have zero optional fields");

obj.z = 40;
func(obj);
types = findTypeForExpression(wrapper, "arg"); 
assert(types.instructionTypeSet.structures[0].fields.length === 2, "arg should still have two fields");
assert(types.instructionTypeSet.structures[0].fields.indexOf("x") !== -1, "arg should have field: 'x'");
assert(types.instructionTypeSet.structures[0].fields.indexOf("y") !== -1, "arg should have field: 'y'");
assert(types.instructionTypeSet.structures[0].optionalFields.length === 1, "arg should have one optional field");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("z") !== -1, "arg should have optional field: 'z'");

obj["foo"] = "type";
obj["baz"] = "profiler";
func(obj);
types = findTypeForExpression(wrapper, "arg"); 
assert(types.instructionTypeSet.structures[0].fields.length === 2, "arg should still have two fields");
assert(types.instructionTypeSet.structures[0].fields.indexOf("x") !== -1, "arg should have field: 'x'");
assert(types.instructionTypeSet.structures[0].fields.indexOf("y") !== -1, "arg should have field: 'y'");
assert(types.instructionTypeSet.structures[0].optionalFields.length === 3, "arg should have three optional field");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("z") !== -1, "arg should have optional field: 'z'");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("foo") !== -1, "arg should have optional field: 'foo'");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("baz") !== -1, "arg should have optional field: 'baz'");

func({});
types = findTypeForExpression(wrapper, "arg"); 
assert(types.instructionTypeSet.structures[0].fields.length === 0, "arg should have no common fields");
assert(types.instructionTypeSet.structures[0].optionalFields.length === 5, "arg should have five optional field");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("x") !== -1, "arg should have optional field: 'x'");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("y") !== -1, "arg should have optional field: 'y'");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("z") !== -1, "arg should have optional field: 'z'");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("foo") !== -1, "arg should have optional field: 'foo'");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("baz") !== -1, "arg should have optional field: 'baz'");
