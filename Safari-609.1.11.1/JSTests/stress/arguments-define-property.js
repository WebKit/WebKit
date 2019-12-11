function assert(a) {
    if (!a)
        throw Error("Bad assertion!");
}

function testProperties(o, initProperty, testProperty, shouldThrow) {
    Object.defineProperty(arguments, 0, initProperty);

    if (shouldThrow) {
        try {
            Object.defineProperty(arguments, 0, testProperty);
            assert(false);
        } catch(e) {
            assert(e instanceof TypeError);
        }
    } else {
        assert(Object.defineProperty(arguments, 0, testProperty));
    }
}

testProperties("foo", {configurable: false}, {writable: true}, false);
testProperties("foo", {configurable: false}, {configurable: true}, true);
testProperties("foo", {configurable: false, enumareble: true}, {enumerable: false}, true);
testProperties("foo", {configurable: false, writable: false}, {writable: false}, false);
testProperties("foo", {configurable: false, writable: false}, {writable: true}, true);
testProperties("foo", {configurable: false, writable: false, value: 50}, {value: 30}, true);
testProperties("foo", {configurable: false, writable: false, value: 30}, {value: 30}, false);
testProperties("foo", {configurable: false, get: () => {return 0}}, {get: () => {return 10}}, true);
let getterFoo = () => {return 0};
testProperties("foo", {configurable: false, get: getterFoo}, {get: getterFoo}, false);
testProperties("foo", {configurable: false, set: (x) => {return 0}}, {get: (x) => {return 10}}, true);
let setterFoo = (x) => {return 0};
testProperties("foo", {configurable: false, set: setterFoo}, {set: setterFoo}, false);

