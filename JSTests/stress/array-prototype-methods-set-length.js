function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!\ncreateTestObject:\n${createTestObject}\nmakeLengthReadOnly: ${makeLengthReadOnly}`);
};

function shouldThrow(func, reExpectedError) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (!reExpectedError.test(error.toString()))
            throw new Error(`Bad error: ${error}!\ncreateTestObject:\n${createTestObject}\nmakeLengthReadOnly: ${makeLengthReadOnly}`);
    }
    if (!errorThrown)
        throw new Error(`Didn't throw!\ncreateTestObject: ${createTestObject}\nmakeLengthReadOnly: ${makeLengthReadOnly}`);
};

var createTestObject;
const createTestObjectFunctions = [
    len => new Array(len),
    len => new Proxy(new Array(len), {}),
    len => { const obj = Object.create(Array.prototype); obj.length = len; return obj; },
];

var makeLengthReadOnly;
const makeLengthReadOnlyFunctions = [
    arr => { Object.freeze(arr); },
    arr => { Object.defineProperty(arr, "length", { writable: false }); },
];

var testObject;
const expectedTypeError = /^TypeError:.+/;

for (createTestObject of createTestObjectFunctions) {
for (makeLengthReadOnly of makeLengthReadOnlyFunctions) {

    testObject = createTestObject(0);
    makeLengthReadOnly(testObject);
    shouldThrow(() => { "use strict"; testObject.length = 0; }, expectedTypeError);
    shouldBe(testObject.length, 0);

    testObject = createTestObject(0);
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.pop(); }, expectedTypeError);
    shouldBe(testObject.length, 0);

    testObject = createTestObject(1);
    testObject[0] = 1;
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.pop(); }, expectedTypeError);
    shouldBe(testObject.length, 1);

    testObject = createTestObject(0);
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.push(); }, expectedTypeError);
    shouldBe(testObject.length, 0);

    testObject = createTestObject(0);
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.push(1); }, expectedTypeError);
    shouldBe(testObject.length, 0);

    testObject = createTestObject(0);
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.shift(); }, expectedTypeError);
    shouldBe(testObject.length, 0);

    testObject = createTestObject(1);
    testObject[0] = 1;
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.shift(); }, expectedTypeError);
    shouldBe(testObject.length, 1);

    testObject = createTestObject(0);
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.unshift(); }, expectedTypeError);
    shouldBe(testObject.length, 0);

    testObject = createTestObject(0);
    makeLengthReadOnly(testObject);
    shouldThrow(() => { testObject.unshift(1); }, expectedTypeError);
    shouldBe(testObject.length, 0);

}}

// let's have a bad time?

for (createTestObject of createTestObjectFunctions) {
for (makeLengthReadOnly of makeLengthReadOnlyFunctions) {

    let arrayPrototypeGet0Calls = 0;
    let arrayPrototypeSet0Calls = 0;

    Object.defineProperty(Array.prototype, "0", {
        get() {
            Object.defineProperty(testObject, "length", { writable: false });
            arrayPrototypeGet0Calls++;
        },
        set(_val) {
            Object.defineProperty(testObject, "length", { writable: false });
            arrayPrototypeSet0Calls++;
        },
        configurable: true,
    });

    testObject = createTestObject(1);
    shouldThrow(() => { testObject.pop(); }, expectedTypeError);
    shouldBe(arrayPrototypeGet0Calls, 1);
    shouldBe(testObject.hasOwnProperty(0), false);
    shouldBe(testObject.length, 1);

    testObject = createTestObject(1);
    shouldThrow(() => { testObject.shift(); }, expectedTypeError);
    shouldBe(arrayPrototypeGet0Calls, 2);
    shouldBe(testObject.hasOwnProperty(0), false);
    shouldBe(testObject.length, 1);

    testObject = createTestObject(0);
    shouldThrow(() => { testObject.push(1); }, expectedTypeError);
    shouldBe(arrayPrototypeSet0Calls, 1);
    shouldBe(testObject.hasOwnProperty(0), false);
    shouldBe(testObject.length, 0);

    testObject = createTestObject(0);
    shouldThrow(() => { testObject.unshift(1); }, expectedTypeError);
    shouldBe(arrayPrototypeSet0Calls, 2);
    shouldBe(testObject.hasOwnProperty(0), false);
    shouldBe(testObject.length, 0);

}}

// [[DefineOwnProperty]] shouldn't fail

for (createTestObject of createTestObjectFunctions) {
for (makeLengthReadOnly of makeLengthReadOnlyFunctions) {

    testObject = createTestObject(42);
    makeLengthReadOnly(testObject);
    Object.defineProperty(testObject, "length", { value: 42 }); // doesn't throw
    shouldBe(Reflect.defineProperty(testObject, "length", { value: 42 }), true);
    shouldBe(testObject.length, 42);

}}
