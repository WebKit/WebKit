let objX = {objProperty: {fetchme: 1234}};
let objY = {doubleProperty: 2130562.5098039214};

function createArray() {
    let protoWithIndexedAccessors = {};
    Object.defineProperty(protoWithIndexedAccessors, 1337, { get() { return 1337; } });

    function helper(i) {
        let a = new Array;
        if (i > 0) {
            Object.setPrototypeOf(a, protoWithIndexedAccessors);
        }
        return a;
    }

    for (let i = 1; i < 10000; i++) {
        helper(i);
    }
    return helper(0);
}

let obj = {};
obj.inlineProperty1 = 1337;
obj.inlineProperty2 = 1338;
obj.oolProperty1 = objX;

let a = createArray();
a.index = 42;
a.input = "foobar";
a.groups = obj;

global = a;
global = a;

Object.defineProperty(Array.prototype, 1337, { get() { return 1337; } });

function foo() {
    return global.groups.oolProperty1.objProperty.fetchme;
}

for (let i = 0; i < 10000; i++) {
    try {
        foo(i);
    } catch { }
}

try {
    let match = "foo".match(/(?<oolProperty1>foo)/);
    match.groups.oolProperty1 = objY;
    global = match;
    foo();
} catch { }
