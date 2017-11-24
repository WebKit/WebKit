var findTypeForExpression = $vm.findTypeForExpression;

load("./driver/driver.js");

function wrapper()
{

var theA = new A;
var theB = new B;
var theC = new C;
var hierarchyTest = new B;
hierarchyTest = new C;

var secondHierarchyTest = Object.create(theB);
secondHierarchyTest = Object.create(theC);

var secondB = Object.create(theB);

function A() { };
function B() { }; B.prototype.__proto__ = A.prototype;
function C() { }; C.prototype.__proto__ = A.prototype;

}
wrapper();

// ====== End test cases ======

types = findTypeForExpression(wrapper, "theA = n");
assert(types.globalTypeSet.displayTypeName === "A", "variable 'theA' should have displayTypeName 'A'");
assert(types.instructionTypeSet.displayTypeName === "A", "variable 'theA' should have displayTypeName 'A'");

types = findTypeForExpression(wrapper, "theB = n");
assert(types.globalTypeSet.displayTypeName === "B", "variable 'theB' should have displayTypeName 'B'");
assert(types.instructionTypeSet.displayTypeName === "B", "variable 'theB' should have displayTypeName 'B'");

types = findTypeForExpression(wrapper, "theC = n");
assert(types.globalTypeSet.displayTypeName === "C", "variable 'theC' should have displayTypeName 'C'");
assert(types.instructionTypeSet.displayTypeName === "C", "variable 'theC' should have displayTypeName 'C'");

types = findTypeForExpression(wrapper, "hierarchyTest = new B;");
assert(types.globalTypeSet.displayTypeName === "A", "variable 'hierarchyTest' should have displayTypeName 'A'");

types = findTypeForExpression(wrapper, "hierarchyTest = new C;");
assert(types.globalTypeSet.displayTypeName === "A", "variable 'hierarchyTest' should have displayTypeName 'A'");

types = findTypeForExpression(wrapper, "secondHierarchyTest = Object.create(theB);");
assert(types.globalTypeSet.displayTypeName === "A", "variable 'secondHierarchyTest' should have displayTypeName 'A'");

types = findTypeForExpression(wrapper, "secondHierarchyTest = Object.create(theC);");
assert(types.globalTypeSet.displayTypeName === "A", "variable 'secondHierarchyTest' should have displayTypeName 'A'");

types = findTypeForExpression(wrapper, "secondB");
assert(types.globalTypeSet.displayTypeName === "B", "variable 'secondB' should have displayTypeName 'B'");
