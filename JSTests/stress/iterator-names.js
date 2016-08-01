function test(object, name) {
    return {
        object,
        name: '[object ' + name + ']'
    };
}

function iter(object) {
    return object[Symbol.iterator]();
}

var tests = [
    test(iter([]), "Array Iterator"),
    test(iter(new Array), "Array Iterator"),
    test([].keys(), "Array Iterator"),
    test([].entries(), "Array Iterator"),
    test(iter(new Map), "Map Iterator"),
    test((new Map()).keys(), "Map Iterator"),
    test((new Map()).entries(), "Map Iterator"),
    test(iter(new Set), "Set Iterator"),
    test((new Set()).keys(), "Set Iterator"),
    test((new Set()).entries(), "Set Iterator"),
    test(iter(new String("")), "String Iterator"),
    test(iter(""), "String Iterator"),
];

function check(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (var { object, name } of tests) {
    check(object.toString(), name);
    check(Object.prototype.toString.call(object), name);
}
