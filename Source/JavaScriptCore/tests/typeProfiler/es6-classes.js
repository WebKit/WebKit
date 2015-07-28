load("./driver/driver.js");

let changeFoo;
let tdzError;
let scoping;
let scoping2;
function noop(){}

function wrapper() {
    class Animal {
        constructor() { }
        methodA() {}
    }
    class Dog extends Animal {
        constructor() { super() }
        methodB() {}
    }

    let dogInstance = new Dog;
    const animalInstance = new Animal;
}
wrapper();

// ====== End test cases ======

var types = findTypeForExpression(wrapper, "dogInstance =");
assert(types.globalTypeSet.displayTypeName === "Dog", "Should show constructor name");
assert(types.globalTypeSet.structures[0].constructorName === "Dog", "Should be Dog");
assert(types.globalTypeSet.structures[0].proto.fields.length === 2, "should have two fields");
assert(types.globalTypeSet.structures[0].proto.fields.indexOf("constructor") !== -1, "should have constructor");
assert(types.globalTypeSet.structures[0].proto.fields.indexOf("methodB") !== -1, "should have methodB");

types = findTypeForExpression(wrapper, "animalInstance =");
assert(types.globalTypeSet.displayTypeName === "Animal", "Should show constructor name");
assert(types.globalTypeSet.structures.length === 1, "should have one structure");
assert(types.globalTypeSet.structures[0].constructorName === "Animal", "Should be Animal");
assert(types.globalTypeSet.structures[0].proto.fields.length === 2, "should have two fields");
assert(types.globalTypeSet.structures[0].proto.fields.indexOf("constructor") !== -1, "should have constructor");
assert(types.globalTypeSet.structures[0].proto.fields.indexOf("methodA") !== -1, "should have methodA");
