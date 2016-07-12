function assertProperError(e) {
    if ((!(e instanceof TypeError)) || e.message.indexOf("Receiver should be a typed array view but was not an object") === -1)
        shouldBeTrue("false");
}

var tArray;

function testIntTypedArray(TypedArray) {

    tArray = new TypedArray([0,2,3]);
    shouldBeTrue("tArray.includes(2)");
    shouldBeTrue("!tArray.includes(4)");
    shouldBeTrue("!tArray.includes(3, 3)");
    shouldBeTrue("tArray.includes(3, -1)");
    shouldBeTrue("tArray.includes(3, {valueOf: () => -1})");

    // Test non-array-native values
    shouldBeTrue("tArray.includes(2.0)");
    shouldBeTrue("!tArray.includes(2.5)");
    shouldBeTrue("!tArray.includes(\"abc\")");
    shouldBeTrue("!tArray.includes(null)");
    shouldBeTrue("!tArray.includes(undefined)");
    shouldBeTrue("!tArray.includes({1: ''})");
    shouldBeTrue("!tArray.includes(\"\")");

    // Testing TypeError handling
    try {
        tArray = new TypedArray([0, 1, 2]);
        tArray.includes.call(null, 2);
    } catch(e) {
        assertProperError(e);
    }

    try {
        tArray = new TypedArray([0, 1, 2]);
        tArray.includes.call(undefined, 2);
    } catch(e) {
        assertProperError(e);
    }

}

testIntTypedArray(Uint8Array);
testIntTypedArray(Int8Array);
testIntTypedArray(Uint8ClampedArray);
testIntTypedArray(Uint16Array);
testIntTypedArray(Int16Array);
testIntTypedArray(Uint32Array);
testIntTypedArray(Int32Array);

var fArray;

function testFloatTypedArray(TypedArray) {

    fArray = new TypedArray([1.0, 2.0 , 3.0]);
    shouldBeTrue("fArray.includes(2.0)");
    shouldBeTrue("!fArray.includes(4.0)");
    shouldBeTrue("!fArray.includes(3.0, 3)");
    shouldBeTrue("fArray.includes(3, -1)");

    fArray = new TypedArray([NaN]);
    shouldBeTrue("!fArray.includes(\"abc\")");
    shouldBeTrue("!fArray.includes(null)");
    shouldBeTrue("!fArray.includes(undefined)");
    shouldBeTrue("!fArray.includes({1: ''})");
    shouldBeTrue("!fArray.includes(\"\")");

    // Testing TypeError handling
    try {
        fArray = new TypedArray([0, 1, 2]);
        fArray.includes.call(null, 2);
    } catch(e) {
        assertProperError(e);
    }

    try {
        fArray = new TypedArray([0, 1, 2]);
        fArray.includes.call(undefined, 2);
    } catch(e) {
        assertProperError(e);
    }

}

// NaN handling (only true for Float32 and Float64)
shouldBeTrue("!(new Uint8Array([NaN]).includes(NaN))");
shouldBeTrue("new Float32Array([NaN]).includes(NaN)");
shouldBeTrue("new Float64Array([NaN]).includes(NaN)");

var descriptor;
var gTypedArray;

function testDescriptor(TypedArray) {
    gTypedArray = TypedArray;
    descriptor = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(TypedArray.prototype), "includes");

    shouldBeTrue("descriptor.configurable");
    shouldBeTrue("descriptor.writable");
    shouldBeTrue("!descriptor.enumerable");
    shouldBeTrue("descriptor.get === undefined");
    shouldBeTrue("descriptor.set === undefined");

    shouldBeTrue("Object.getPrototypeOf(gTypedArray.prototype).includes.name === \"includes\"");
    shouldBeTrue("Object.getPrototypeOf(gTypedArray.prototype).includes.length === 1");
}

testDescriptor(Uint8Array);
testDescriptor(Int8Array);
testDescriptor(Uint8ClampedArray);
testDescriptor(Uint16Array);
testDescriptor(Int16Array);
testDescriptor(Uint32Array);
testDescriptor(Int32Array);
testDescriptor(Float32Array);
testDescriptor(Float64Array);

// Testing boundaries

var arr = new Uint8Array([0, 254]);
shouldBeTrue("arr.includes(0)");
shouldBeTrue("arr.includes(254)");
shouldBeTrue("!arr.includes(255)");
shouldBeTrue("!arr.includes(-1)");

arr = new Int8Array([-128, 127]);
shouldBeTrue("arr.includes(-128)");
shouldBeTrue("arr.includes(127)");
shouldBeTrue("!arr.includes(128)");
shouldBeTrue("!arr.includes(-129)");

arr = new Uint8ClampedArray([-128, 256]);
shouldBeTrue("arr.includes(255)");
shouldBeTrue("arr.includes(0)");
shouldBeTrue("!arr.includes(-128)");
shouldBeTrue("!arr.includes(-256)");

arr = new Uint16Array([0, 65535]);
shouldBeTrue("arr.includes(0)");
shouldBeTrue("arr.includes(65535)");
shouldBeTrue("!arr.includes(65536)");
shouldBeTrue("!arr.includes(-1)");

arr = new Int16Array([-32768, 32767]);
shouldBeTrue("arr.includes(-32768)");
shouldBeTrue("arr.includes(32767)");
shouldBeTrue("!arr.includes(32768)");
shouldBeTrue("!arr.includes(-32769)");

arr = new Uint32Array([0, 4294967295]);
shouldBeTrue("arr.includes(0)");
shouldBeTrue("arr.includes(4294967295)");
shouldBeTrue("!arr.includes(4294967296)");
shouldBeTrue("!arr.includes(-1)");

arr = new Int32Array([-2147483648, 2147483647]);
shouldBeTrue("arr.includes(-2147483648)");
shouldBeTrue("arr.includes(2147483647)");
shouldBeTrue("!arr.includes(2147483648)");
shouldBeTrue("!arr.includes(-2147483649)");

arr = new Float32Array([-3.402820018375656e+38, 3.402820018375656e+38]);
shouldBeTrue("arr.includes(-3.402820018375656e+38)");
shouldBeTrue("arr.includes(3.402820018375656e+38)");
shouldBeTrue("!arr.includes(3.50282e+38)");
shouldBeTrue("!arr.includes(-3.50282e+38)");

arr = new Float64Array([-1.79769e+308, 1.79769e+308]);
shouldBeTrue("arr.includes(-1.79769e+308)");
shouldBeTrue("arr.includes(1.79769e+308)");
shouldBeTrue("!arr.includes(-1.89769e+308)");
shouldBeTrue("!arr.includes(1.89769e+308)");

// Testing Infinity

function testInfinity(TypedArray) {
    arr = new TypedArray([Infinity]);
    shouldBeTrue("arr.includes(Infinity)");
    shouldBeTrue("!arr.includes(-Infinity)");
    shouldBeTrue("!arr.includes(NaN)");

    arr = new TypedArray([-Infinity]);
    shouldBeTrue("arr.includes(-Infinity)");
    shouldBeTrue("!arr.includes(Infinity)");
    shouldBeTrue("!arr.includes(NaN)");
}

testInfinity(Float32Array);
testInfinity(Float64Array);

// Test float->double precision

shouldBeTrue("!(new Float32Array([2.40282e+38]).includes(2.40282e+38))");

// Checking spec
var funcCallCount;

function checkingValueOfCall(TypedArray) {
    tArray = new TypedArray([0, 1, 2]);
    funcCallCount = {callCount: 0, valueOf: function() {this.callCount++; return 0;}};

    tArray.includes(0, funcCallCount);
    shouldBeTrue("funcCallCount.callCount === 1");

    tArray.includes("abc", funcCallCount);
    shouldBeTrue("funcCallCount.callCount === 2");

    tArray.includes(null, funcCallCount);
    shouldBeTrue("funcCallCount.callCount === 3");

    tArray.includes(undefined, funcCallCount);
    shouldBeTrue("funcCallCount.callCount === 4");

    tArray.includes({1: ''}, funcCallCount);
    shouldBeTrue("funcCallCount.callCount === 5");

    tArray.includes("", funcCallCount);
    shouldBeTrue("funcCallCount.callCount === 6");
}

checkingValueOfCall(Uint8Array);
checkingValueOfCall(Int8Array);
checkingValueOfCall(Uint8ClampedArray);
checkingValueOfCall(Uint16Array);
checkingValueOfCall(Int16Array);
checkingValueOfCall(Uint32Array);
checkingValueOfCall(Int32Array);
checkingValueOfCall(Float32Array);
checkingValueOfCall(Float64Array);
