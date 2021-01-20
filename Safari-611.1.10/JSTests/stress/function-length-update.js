function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function userFunction(a, b, c) { }

shouldBe(userFunction.length, 3);
shouldBe(userFunction.bind().length, 3);
userFunction.length = 4;
shouldBe(userFunction.length, 3); // Because it is ReadOnly
shouldBe(userFunction.bind().length, 3);
delete userFunction.length;
shouldBe(userFunction.length, 0);
Object.defineProperty(userFunction, "length", {
    writable: true,
    configurable: true,
    value: 4
});
shouldBe(userFunction.length, 4);
shouldBe(userFunction.bind().length, 4);

var hostFunction = String.prototype.replace;
shouldBe(hostFunction.length, 2);
shouldBe(hostFunction.bind().length, 2);
hostFunction.length = 4;
shouldBe(hostFunction.length, 2); // Because it is ReadOnly
shouldBe(hostFunction.bind().length, 2);
delete hostFunction.length;
shouldBe(hostFunction.length, 0);
Object.defineProperty(hostFunction, "length", {
    writable: true,
    configurable: true,
    value: 4
});
shouldBe(hostFunction.length, 4);
shouldBe(hostFunction.bind().length, 4);

function userFunction2(a, b, c) { }

var boundFunction = userFunction2.bind();
shouldBe(boundFunction.length, 3);
shouldBe(boundFunction.bind().length, 3);
boundFunction.length = 4;
shouldBe(boundFunction.length, 3); // Because it is ReadOnly
shouldBe(boundFunction.bind().length, 3);
delete boundFunction.length;
shouldBe(boundFunction.length, 0);
Object.defineProperty(boundFunction, "length", {
    writable: true,
    configurable: true,
    value: 4
});
shouldBe(boundFunction.length, 4);
shouldBe(boundFunction.bind().length, 4);
