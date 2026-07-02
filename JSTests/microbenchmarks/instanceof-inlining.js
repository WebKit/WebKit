function test(target)
{
    return target instanceof Object;
}
noInline(test);

class A {
}

class B extends A {
}

class C extends B {
}

var array = [
    {},
    function () { },
    new A,
    new B,
    new C,
    [],
    new Map(),
    new Set(),
    new Date(),
];
for (var i = 0; i < 1e6; ++i) {
    for (let value of array) {
        test(value);
    }
}
