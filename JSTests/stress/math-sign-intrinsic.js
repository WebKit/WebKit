function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + String(actual) + ' expected: ' + String(expected));
}

function signInt32(value) {
    return Math.sign(value | 0);
}
noInline(signInt32);

function signDouble(value) {
    return Math.sign(value);
}
noInline(signDouble);

function signUntyped(value) {
    return Math.sign(value);
}
noInline(signUntyped);

function signNoArgument() {
    return Math.sign();
}
noInline(signNoArgument);

function signProduct(a, b) {
    return Math.sign(a * b);
}
noInline(signProduct);

function signWideInt(value) {
    return Math.sign(value);
}
noInline(signWideInt);

function signTypedArray(view, index) {
    return Math.sign(view[index]);
}
noInline(signTypedArray);

function signInt32Loop(limit) {
    var acc = 0;
    for (var j = -limit; j <= limit; ++j)
        acc = (acc + Math.sign(j)) | 0;
    return acc;
}
noInline(signInt32Loop);

function signBooleanOrInt(value) {
    return Math.sign(value);
}
noInline(signBooleanOrInt);

function signConstants(flag, obj) {
    var a = Math.sign(-12);
    var b = Math.sign(-0.0);
    var c = Math.sign("-0");
    var d = Math.sign(undefined);
    var e = flag ? Math.sign(obj) + Math.sign(obj) : 0;
    return [a, b, c, d, e];
}
noInline(signConstants);

function signExtraArgument(value, ignored) {
    return Math.sign(value, ignored);
}
noInline(signExtraArgument);

var int32Inputs = [0, 1, -1, 42, -42, 2147483647, -2147483648];
var int32Expected = [0, 1, -1, 1, -1, 1, -1];
var doubleInputs = [0.5, -0.5, 0, -0, NaN, Infinity, -Infinity, 1e-300, -1e-300, 3];
var doubleExpected = [1, -1, 0, -0, NaN, 1, -1, 1, -1, 1];
var boolOrIntInputs = [true, false, 0, -5, 7];
var boolOrIntExpected = [1, 0, 0, -1, 1];

var nanBuffer = new ArrayBuffer(32);
var nanU32 = new Uint32Array(nanBuffer);
var nanF64 = new Float64Array(nanBuffer);
nanU32[0] = 0x41414140;
nanU32[1] = 0xfffe0001;
nanU32[2] = 0x0000002a;
nanU32[3] = 0xffff0000;
nanF64[2] = -0.0;
nanF64[3] = -2.5;
var nanExpected = [NaN, NaN, -0.0, -1];

var valueOfCalls = 0;
var object = { valueOf() { ++valueOfCalls; return -7; } };
var untypedInputs = ["5", "-0", "", "abc", null, undefined, true, false, object, -3.5, 4];
var untypedExpected = [1, -0, 0, NaN, 0, NaN, 1, 0, -1, -1, 1];
var sideEffectCounter = 0;
var sideEffectObj = { valueOf() { ++sideEffectCounter; return sideEffectCounter & 1 ? 3 : -3; } };
var unreachedArg = { valueOf() { throw new Error('second argument should not be converted'); } };

for (var i = 0; i < testLoopCount; ++i) {
    var index = i % int32Inputs.length;
    shouldBe(signInt32(int32Inputs[index]), int32Expected[index]);
    index = i % doubleInputs.length;
    shouldBe(signDouble(doubleInputs[index]), doubleExpected[index]);
    index = i % untypedInputs.length;
    shouldBe(signUntyped(untypedInputs[index]), untypedExpected[index]);
    index = i % boolOrIntInputs.length;
    shouldBe(signBooleanOrInt(boolOrIntInputs[index]), boolOrIntExpected[index]);
    index = i & 3;
    var typedResult = signTypedArray(nanF64, index);
    shouldBe(typeof typedResult, 'number');
    shouldBe(typedResult, nanExpected[index]);
    shouldBe(signInt32Loop(3), 0);
    var folded = signConstants((i & 15) === 0, sideEffectObj);
    shouldBe(folded[0], -1);
    shouldBe(folded[1], -0.0);
    shouldBe(folded[2], -0.0);
    shouldBe(folded[3], NaN);
    if ((i & 15) === 0)
        shouldBe(folded[4], 0);
    shouldBe(signExtraArgument(-9, unreachedArg), -1);
    shouldBe(signNoArgument(), NaN);
}

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(signProduct(i, 2), i ? 1 : 0);
    shouldBe(signProduct(-i - 1, 2), -1);
    shouldBe(signWideInt(i + 1), 1);
}
shouldBe(signProduct(-1, 0), -0.0);
shouldBe(signProduct(0, -1), -0.0);
shouldBe(signProduct(0, 0), 0);
shouldBe(signWideInt(2 ** 40), 1);
shouldBe(signWideInt(-(2 ** 40)), -1);
shouldBe(signWideInt(-(2 ** 53)), -1);

if (valueOfCalls === 0)
    throw new Error('valueOf was not called');

var threw = false;
try {
    signUntyped(Symbol());
} catch (error) {
    threw = error instanceof TypeError;
}
shouldBe(threw, true);

threw = false;
try {
    signUntyped(1n);
} catch (error) {
    threw = error instanceof TypeError;
}
shouldBe(threw, true);
