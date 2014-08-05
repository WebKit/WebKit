someGlobal = 0;

function foo() {
    var myObject = {};
    for (var i = 0; i < 10000000; ++i) {
        someGlobal = myObject.undefinedProperty;
    }
}
foo();
