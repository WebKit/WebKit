function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

shouldBe(Reflect.enumerate.length, 1);

shouldThrow(() => {
    Reflect.enumerate("hello");
}, `TypeError: Reflect.enumerate requires the first argument be an object`);

var iterator = Reflect.enumerate({});
var iteratorPrototype = [][Symbol.iterator]().__proto__.__proto__;
shouldBe(iterator.__proto__ === iteratorPrototype, true);
shouldBe(iterator.hasOwnProperty('next'), true);
shouldBe(iterator.next.length, 0);
shouldBe(iterator[Symbol.iterator]() === iterator, true);

function testIterator(object, expected) {
    var index = 0;
    for (var name of Reflect.enumerate(object))
        shouldBe(name === expected[index++], true);
    shouldBe(index, expected.length);
}

testIterator({ hello:42, 0: 0 }, ['0', 'hello']);
testIterator({ 1:1, hello:42, 0: 0 }, ['0', '1', 'hello']);
testIterator({ 1:1, hello:42, 0: 0, world: 'ok', 100000:0 }, ['0', '1', '100000', 'hello', 'world']);

testIterator({ 1:1, hello:42, 0: 0, [Symbol.unscopables]: 42, world: 'ok', 100000:0 }, ['0', '1', '100000', 'hello', 'world']);

var object = { 1:1, hello:42, 0: 0, [Symbol.unscopables]: 42, world: 'ok', 100000:0 };
Object.defineProperty(object, 'hidden', {
    value: 42,
    enumerable: false
});
testIterator(object, ['0', '1', '100000', 'hello', 'world']);

testIterator({ hello:42, 0: 0, __proto__: { 1: 1, world: 42 } }, ['0', 'hello', '1', 'world']);
testIterator({}, []);
testIterator([], []);

(function () {
    var object = { hello: 42, world: 50 };
    var iterator = Reflect.enumerate(object);
    iterator.next();
    delete object.hello;
    delete object.world;
    shouldBe(iterator.next().done, true);
}());

(function () {
    var proto = { ng: 200 };
    var object = { __proto__: proto, hello: 42, world: 50 };
    var iterator = Reflect.enumerate(object);
    iterator.next();
    delete proto.ng;
    shouldBe(iterator.next().value !== 'ng', true);
    shouldBe(iterator.next().done, true);
}());

(function () {
    var proto = { ng: 200 };
    var object = { __proto__: proto, world: 50 };
    var iterator = Reflect.enumerate(object);
    iterator.next();
    delete proto.world;
    shouldBe(iterator.next().value, 'ng');
    shouldBe(iterator.next().done, true);
}());
