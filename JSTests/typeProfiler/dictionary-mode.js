load("./driver/driver.js");

function wrapper()
{

var foo = {};
for (var i = 0; i < 150; i++) {
    foo["hello" + i] = i;
}
var shouldBeInDictionaryMode = foo;

var shouldNotBeInDictionaryMode = {
    "1": 1,
    "2": 2,
    "3": 3
}

}
wrapper();

var types = findTypeForExpression(wrapper, "shouldBeInDictionaryMode"); 
assert(types.globalTypeSet.structures.length === 1, "Should have one structure.");
assert(types.globalTypeSet.structures[0].isInDictionaryMode, "Should be in dictionary mode");

types = findTypeForExpression(wrapper, "shouldNotBeInDictionaryMode"); 
assert(types.globalTypeSet.structures.length === 1, "Should have one structure.");
assert(!types.globalTypeSet.structures[0].isInDictionaryMode, "Should not be in dictionary mode");
