var regExp = /(?:)/;
var date = new Date();
var map = new Map();
var weakSet = new WeakSet();
var promise = Promise.resolve(1);

function testObjectUse(object) {
    return Reflect.getPrototypeOf(object);
}
noInline(testObjectUse);

function testDefaultCase(value) {
    return Object.getPrototypeOf(value);
}
noInline(testDefaultCase);

function test() {
    testObjectUse(regExp);
    testObjectUse(date);
    testObjectUse(map);
    testObjectUse(weakSet);
    testObjectUse(promise);

    testDefaultCase(regExp);
    testDefaultCase(date);
    testDefaultCase(map);
    testDefaultCase(weakSet);
    testDefaultCase(promise);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();
