
function assert(b) {
    if (!b)
        throw new Error("Bad")
}

var Test = function(){};

let methodNumber = 0;
function addMethods() {
    const methodCount = 65;
    for (var i = 0; i < methodCount; i++){
        Test.prototype['myMethod' + i + methodNumber] = function(){};
        ++methodNumber;
    }
}

addMethods();

var test1 = new Test();

for (var k in test1) { }

let test2 = new Test();

for (let i = 0; i < 100; ++i ) {
    let propName = 'myAdditionalMethod' + i;
    Test.prototype[propName] = function(){};
    let foundNewPrototypeProperty = false;
    for (let k in test2) {
        if (propName === k)
            foundNewPrototypeProperty = true;
    }
    assert(foundNewPrototypeProperty);
    addMethods();
}
