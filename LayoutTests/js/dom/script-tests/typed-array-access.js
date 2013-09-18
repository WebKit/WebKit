description(
"Tests that accesses to typed arrays handle value conversions correctly."
);

// FIXME: This test assumes little-endian. It would take some hackage to convert
// it to a big-endian test.

function bitsToString(array) {
    return Array.prototype.join.call(array, ":");
}
    
// Call this like so:
//
//    bitsToValue(type, expectedJavascriptValue, first32Bits, second32Bits)
//
// Note that the second32Bits are optional.
//
// Where 'type' is for example "Int32".
function bitsToValue(type, expected) {
    var baseArray = new Int32Array(arguments.length - 2);
    for (var i = 0; i < arguments.length - 2; ++i)
        baseArray[i] = arguments[i + 2];
    
    var actual = (new this[type + "Array"](baseArray.buffer, 0, 1))[0];
    if (isResultCorrect(actual, expected))
        testPassed("Reading bit pattern " + bitsToString(baseArray) + " with " + type + " resulted in " + stringify(actual));
    else
        testFailed("Reading bit pattern " + bitsToString(baseArray) + " with " + type + " should result in " + stringify(expected) + " but instead was " + stringify(actual));
}

// Same signature as bitsToValue, except that the meaning is reversed.
function valueToBits(type, input) {
    var buffer = new ArrayBuffer((arguments.length - 2) * 4);
    
    var baseArray = new this[type + "Array"](buffer);
    baseArray[0] = input;
    
    var expectedArray = new Int32Array(arguments.length - 2);
    for (var i = 0; i < arguments.length - 2; ++i)
        expectedArray[i] = arguments[i + 2];
    
    var actualArray = new Int32Array(buffer);
    if (isResultCorrect(actualArray, expectedArray))
        testPassed("Writing the value " + stringify(input) + " with type " + type + " results in bit pattern " + bitsToString(actualArray));
    else
        testFailed("Writing the value " + stringify(input) + " with type " + type + " should result in bit pattern " + bitsToString(expectedArray) + " but instead was " + bitsToString(actualArray));
}

// Test that both directions work.
function roundTrip() {
    bitsToValue.apply(this, arguments);
    valueToBits.apply(this, arguments);
}

roundTrip("Int8", 0, 0);
roundTrip("Int8", 42, 42);
roundTrip("Int8", -1, 255);
bitsToValue("Int8", 112, -400);
bitsToValue("Int8", -112, 400);
valueToBits("Int8", 1000, 232);
valueToBits("Int8", -1000, 24);
valueToBits("Int8", 0.5, 0);
valueToBits("Int8", -0.5, 0);
valueToBits("Int8", -1.5, 255);
valueToBits("Int8", 6326266464264213, 21);
valueToBits("Int8", -6326266464264213, 235);

roundTrip("Uint8", 0, 0);
roundTrip("Uint8", 42, 42);
roundTrip("Uint8", 255, 255);
bitsToValue("Uint8", 144, 400);
bitsToValue("Uint8", 112, -400);
valueToBits("Uint8", 1000, 232);
valueToBits("Uint8", -1000, 24);
valueToBits("Uint8", 0.5, 0);
valueToBits("Uint8", -0.5, 0);
valueToBits("Uint8", -1.5, 255);
valueToBits("Uint8", 6326266464264213, 21);
valueToBits("Uint8", -6326266464264213, 235);

roundTrip("Uint8Clamped", 0, 0);
roundTrip("Uint8Clamped", 42, 42);
roundTrip("Uint8Clamped", 255, 255);
bitsToValue("Uint8Clamped", 144, 400);
bitsToValue("Uint8Clamped", 112, -400);
valueToBits("Uint8Clamped", 1000, 255);
valueToBits("Uint8Clamped", -1000, 0);
valueToBits("Uint8Clamped", 0.5, 0);
valueToBits("Uint8Clamped", -0.5, 0);
valueToBits("Uint8Clamped", -1.5, 0);
valueToBits("Uint8Clamped", 6326266464264213, 255);
valueToBits("Uint8Clamped", -6326266464264213, 0);

roundTrip("Int16", 0, 0);
roundTrip("Int16", 42, 42);
roundTrip("Int16", 400, 400);
roundTrip("Int16", -1, 65535);
roundTrip("Int16", -400, 65136);
roundTrip("Int16", 30901, 30901);
bitsToValue("Int16", 6784, 400000);
bitsToValue("Int16", -6784, -400000);
valueToBits("Int16", 100000, 34464);
valueToBits("Int16", -100000, 31072);
valueToBits("Int16", 0.5, 0);
valueToBits("Int16", -0.5, 0);
valueToBits("Int16", -1.5, 65535);
valueToBits("Int16", 6326266464264213, 24597);
valueToBits("Int16", -6326266464264213, 40939);

roundTrip("Uint16", 0, 0);
roundTrip("Uint16", 42, 42);
roundTrip("Uint16", 400, 400);
roundTrip("Uint16", 30901, 30901);
bitsToValue("Uint16", 6784, 400000);
bitsToValue("Uint16", 58752, -400000);
valueToBits("Int16", 100000, 34464);
valueToBits("Int16", -100000, 31072);
valueToBits("Uint16", -1, 65535);
valueToBits("Uint16", -400, 65136);
valueToBits("Uint16", 0.5, 0);
valueToBits("Uint16", 6326266464264213, 24597);
valueToBits("Uint16", -6326266464264213, 40939);

roundTrip("Int32", 0, 0);
roundTrip("Int32", 42, 42);
roundTrip("Int32", 1000000000, 1000000000);
roundTrip("Int32", -42, -42);
roundTrip("Int32", -1000000000, -1000000000);
valueToBits("Int32", 0.5, 0);
valueToBits("Int32", -0.5, 0);
valueToBits("Int32", -1.5, -1);
valueToBits("Int32", 6326266464264213, -1319411691);
valueToBits("Int32", -6326266464264213, 1319411691);

roundTrip("Uint32", 0, 0);
roundTrip("Uint32", 42, 42);
roundTrip("Uint32", 1000000000, 1000000000);
valueToBits("Uint32", -42, -42);
bitsToValue("Uint32", (-42)>>>0, -42);
valueToBits("Uint32", -1000000000, -1000000000);
bitsToValue("Uint32", (-1000000000)>>>0, -1000000000);
valueToBits("Uint32", 0.5, 0);
valueToBits("Uint32", -0.5, 0);
valueToBits("Uint32", -1.5, -1);
valueToBits("Uint32", 6326266464264213, -1319411691);
valueToBits("Uint32", -6326266464264213, 1319411691);

roundTrip("Float32", 0, 0);
roundTrip("Float32", 1.5, 1069547520);
valueToBits("Float32", 1000000000, 1315859240);

roundTrip("Float64", 0, 0, 0);
roundTrip("Float64", 1.5, 0, 1073217536);
roundTrip("Float64", 1000000000, 0, 1104006501);
