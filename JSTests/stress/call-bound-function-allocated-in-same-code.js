function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function target(a, b) {
    "use strict";
    return [this, a, b];
}

function callBound(object, a, b) {
    var bound = target.bind(object);
    return bound(a, b);
}
noInline(callBound);

function callBoundTail(object, a) {
    "use strict";
    var bound = target.bind(object);
    return bound(a);
}
noInline(callBoundTail);

function callBoundWithArgument(object, a) {
    var bound = target.bind(object, 42);
    return bound(a);
}
noInline(callBoundWithArgument);

function constructBound(a) {
    var bound = Point.bind({ ignored: true }, a);
    return new bound(a + 1);
}
noInline(constructBound);

function Point(x, y) {
    this.x = x;
    this.y = y;
}

function iterate(array) {
    var iterator = array[Symbol.iterator]();
    var next = iterator.next.bind(iterator);
    var sum = 0;
    for (var step; !(step = next()).done;)
        sum += step.value;
    return sum;
}
noInline(iterate);

function sloppyTarget() {
    return this;
}

function callSloppyBound(value) {
    var bound = sloppyTarget.bind(value);
    return bound();
}
noInline(callSloppyBound);

function callNestedBound(object, other, a) {
    var bound = target.bind(object).bind(other);
    return bound(a);
}
noInline(callNestedBound);

var proxyTarget = new Proxy(target, { });
function callProxyBound(object, a) {
    var bound = proxyTarget.bind(object);
    return bound(a, "p");
}
noInline(callProxyBound);

function callHostBound(a, b) {
    var bound = Math.max.bind(null);
    return bound(a, b);
}
noInline(callHostBound);

function thrower() {
    "use strict";
    throw this;
}

function callThrowingBound(object) {
    var bound = thrower.bind(object);
    try {
        bound();
    } catch (error) {
        return error;
    }
    return null;
}
noInline(callThrowingBound);

for (var i = 0; i < testLoopCount; ++i) {
    var object = { i };
    var result = callBound(object, i, "b");
    shouldBe(result[0], object);
    shouldBe(result[1], i);
    shouldBe(result[2], "b");

    result = callBoundTail(object, i);
    shouldBe(result[0], object);
    shouldBe(result[1], i);
    shouldBe(result[2], undefined);

    result = callBoundWithArgument(object, i);
    shouldBe(result[0], object);
    shouldBe(result[1], 42);
    shouldBe(result[2], i);

    var point = constructBound(i);
    shouldBe(point.x, i);
    shouldBe(point.y, i + 1);
    shouldBe(point instanceof Point, true);

    shouldBe(iterate([1, 2, 3, i]), 6 + i);

    var primitive = callBound(i, 1, 2);
    shouldBe(primitive[0], i);

    var boxed = callSloppyBound(i);
    shouldBe(typeof boxed, "object");
    shouldBe(boxed.valueOf(), i);
    shouldBe(callSloppyBound(undefined), globalThis);

    result = callNestedBound(object, primitive, i);
    shouldBe(result[0], object);
    shouldBe(result[1], i);

    result = callProxyBound(object, i);
    shouldBe(result[0], object);
    shouldBe(result[1], i);
    shouldBe(result[2], "p");

    shouldBe(callHostBound(i, -1), i);

    shouldBe(callThrowingBound(object), object);
}
