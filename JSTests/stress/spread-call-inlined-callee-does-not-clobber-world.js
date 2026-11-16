//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function makeObject(x) {
    return { x };
}
({ x: 1 }).extra = 1;

function add(a, b) { return a + b; }
function count() { return arguments.length; }

function checkAroundCall(object, a, b) {
    let before = object.x;
    let sum = add(...[a, b]);
    return before + sum + object.x;
}
noInline(checkAroundCall);

function checkAroundCallWithArray(object, array) {
    let before = object.x + array.length;
    let sum = add(...array);
    let length = count(...array);
    return before + sum + object.x + length;
}
noInline(checkAroundCallWithArray);

function change(target, a, b) {
    delete target.x;
    target.z = 1000;
    target.x = a + b;
    return arguments.length;
}
function changeStructureInCallee(object, a, b) {
    let before = object.x;
    let length = change(...[object, a, b]);
    return before * 10000 + object.x * 10 + length;
}
noInline(changeStructureInCallee);

function opaque(object) {
    delete object.x;
    object.z = 1000;
    object.x = 7;
}
noInline(opaque);
function callOpaque(object, a) {
    opaque(object);
    return arguments.length + a;
}
function changeStructureInNestedCall(object, a) {
    let before = object.x;
    let result = callOpaque(...[object, a]);
    return before * 10000 + result * 10 + object.x;
}
noInline(changeStructureInNestedCall);

function mutateBetweenCalls(a, b, value) {
    let array = [a, b];
    let first = add(...array);
    array[0] = value;
    let second = add(...array);
    array.push(value);
    return first * 10000 + second * 10 + count(...array);
}
noInline(mutateBetweenCalls);

function truncate(array, first, second) {
    array[1] = 100;
    array.length = 1;
    return first * 10 + second + arguments.length * 100;
}
function mutateInCallee(a, b) {
    let array = [null, a, b];
    array[0] = array;
    return truncate(...array) + array.length * 1000;
}
noInline(mutateInCallee);

function checkAroundIterable(object, iterable) {
    let before = object.x;
    let sum = add(...iterable);
    return before * 100 + sum * 10 + object.x;
}
noInline(checkAroundIterable);

function loop(object, array) {
    let result = 0;
    for (let i = 0; i < 10; ++i)
        result += object.x + array[0] + add(...array) + count(...array);
    return result;
}
noInline(loop);

function forwardRest(...args) { return add(...args) * 10 + count(...args); }
function callForwardRest(a, b, c) { return forwardRest(a, b, c) + a; }
noInline(callForwardRest);
function forwardRestTwice(...args) { return forwardRest(...args); }
function callForwardRestTwice(array, a) {
    let result = array.length + forwardRestTwice(...array);
    return result + a;
}
noInline(callForwardRestTwice);

function sloppyArguments(a, b) {
    a = a + 1;
    let sum = add(...[b, b]);
    return sum * 100 + arguments[0] * 10 + arguments[1];
}
noInline(sloppyArguments);

function functionArguments(a, b) {
    let array = [a, b];
    a = 5;
    let result = readCallerArguments(...array);
    return result + a;
}
function readCallerArguments(first) {
    return functionArguments.arguments.length * 10 + first;
}
noInline(functionArguments);

class Base {
    constructor(a, b) { this.sum = a + b; }
}
class Derived extends Base {
    constructor(...args) { super(...args); }
}
function construct(object, a, b) {
    let before = object.x;
    return before + new Base(...[a, b]).sum + new Derived(a, b).sum + object.x;
}
noInline(construct);

function checkAroundSet(object, set) {
    let before = object.x;
    let sum = add(...set);
    return before + sum + object.x;
}
noInline(checkAroundSet);

function catchThrow(object, a) {
    let before = object.x;
    try {
        before++;
        return callThrow(...[object, a]);
    } catch (error) {
        return before * 100 + error * 10 + object.x;
    }
}
function callThrow(object, a) {
    if (a > 1)
        throw a;
    return a + object.x;
}
noInline(catchThrow);

function exits(object, array) {
    let before = object.x + array.length;
    object.x++;
    let length = count(...array);
    return length * 10 + object.x + array.length - before;
}
noInline(exits);

function locals(a, b, c, array) {
    let x = a + b;
    let y = b + c;
    let sum = array.length + add(...array);
    return [x, y, sum, a, b, c].join();
}
noInline(locals);

let constant = 1;
function readConstant(a) { return constant * 10 + a; }
function callReadConstant(iterable) { return readConstant(...iterable); }
noInline(callReadConstant);
function* changeConstant(value) {
    if (value)
        constant = value;
    yield 5;
}

function* generate(object) {
    yield 1;
    delete object.x;
    object.z = 1000;
    object.x = 2;
    yield 2;
}

const int32s = [2, 3];
const notCopyOnWrite = new Array(2).fill(1);
const doubles = [2.5, 3];
const set = new Set([2, 3]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(checkAroundCall(makeObject(1), 2, 3), 7);
    shouldBe(checkAroundCall(makeObject(1), 2.5, 3), 7.5);
    shouldBe(checkAroundCallWithArray(makeObject(1), int32s), 11);
    shouldBe(checkAroundCallWithArray(makeObject(1), doubles), 11.5);
    shouldBe(changeStructureInCallee(makeObject(1), 2, 3), 10053);
    shouldBe(changeStructureInNestedCall(makeObject(1), 2), 10047);
    shouldBe(mutateBetweenCalls(1, 2, 5), 30073);
    shouldBe(mutateInCallee(1, 2), 1312);
    shouldBe(loop(makeObject(1), int32s), 100);
    shouldBe(callForwardRest(1, 2, 3), 34);
    shouldBe(callForwardRestTwice(int32s, 1), 55);
    shouldBe(sloppyArguments(1, 2), 422);
    shouldBe(functionArguments(1, 2), 26);
    shouldBe(construct(makeObject(1), 2, 3), 12);
    shouldBe(checkAroundSet(makeObject(1), set), 7);
    shouldBe(catchThrow(makeObject(1), i & 3 ? 1 : 2), i & 3 ? 2 : 221);
    shouldBe(exits(makeObject(1), notCopyOnWrite), 21);
    shouldBe(locals(1, 2, 3, notCopyOnWrite), "3,5,4,1,2,3");
    shouldBe(callReadConstant(changeConstant(0)), 15);
    let object = makeObject(1);
    shouldBe(checkAroundIterable(object, generate(object)), 132);
}

shouldBe(callReadConstant(changeConstant(2)), 25);
const tooLarge = new Array(0x100000 + 1).fill(1);
{
    let object = makeObject(1);
    let error;
    try {
        exits(object, tooLarge);
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof RangeError, true);
    shouldBe(object.x, 2);
}
const large = new Array(1000).fill(1);
for (let i = 0; i < 5; ++i) {
    shouldBe(exits(makeObject(1), large), 10001);
    shouldBe(locals(1, 2, 3, large), "3,5,1002,1,2,3");
}

const arrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());
const originalNext = arrayIteratorPrototype.next;
let target;
arrayIteratorPrototype.next = function () {
    let x = target.x;
    delete target.x;
    target.z = 1000;
    target.x = x + 1;
    return originalNext.call(this);
};
for (let i = 0; i < 100; ++i) {
    target = makeObject(1);
    shouldBe(checkAroundIterable(target, int32s), 154);
    target = makeObject(1);
    shouldBe(checkAroundCall(target, 2, 3), 10);
}
