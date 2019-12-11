//@ slow!

function testArray(arrayType)
{
    var testCode =
        `
        // We make this look like a polymorphic types for incomingObject but the GetByVal are never actually
        // polymorphic. The boolean isTypedArray let us differentiate the types.
        function ${ arrayType }AndObjectSpeculationInBounds(incomingObject, iterationLength, isTypedArray) {
            var output = 0;
            output += incomingObject.length;

            if (isTypedArray) {
                for (var i = 0; i < iterationLength; ++i) {
                    output += incomingObject[i];
                }
            } else {
                for (var i = 0; i < iterationLength; ++i) {
                    output += incomingObject[i];
                }
            }
            return output;
        }
        noInline(${ arrayType }AndObjectSpeculationInBounds);

        var typedArray = new ${ arrayType }(64);
        var regularArray = new Array(64);
        for (var i = 0; i < 64; ++i) {
            typedArray[i] = i;
            regularArray[i] = i;
        }

        // Access in bounds.
        for (var i = 0; i < 1e4; ++i) {
            var output = ${ arrayType }AndObjectSpeculationInBounds(typedArray, 64, true);
            if (output !== 32 * 65)
                throw "${ arrayType }AndObjectSpeculationInBounds(typedArray, 64, true) failed, value = " + output;

            var output = ${ arrayType }AndObjectSpeculationInBounds(regularArray, 64, false);
            if (output !== 32 * 65)
                throw "${ arrayType }AndObjectSpeculationInBounds(regularArray, 64, false) failed, value = " + output;
        }

        // One out of bounds on top of the in bounds profile.
        {
            var output = ${ arrayType }AndObjectSpeculationInBounds(typedArray, 128, true);
            if (output === output)
                throw "${ arrayType }AndObjectSpeculationInBounds(typedArray, 128, true) failed, value = " + output;

            var output = ${ arrayType }AndObjectSpeculationInBounds(regularArray, 128, false);
            if (output === output)
                throw "${ arrayType }AndObjectSpeculationInBounds(regularArray, 128, false) failed, value = " + output;
        }

        // Same but here we make out-of-bounds a normal case.
        function ${ arrayType }AndObjectSpeculationOutOfBounds(incomingObject, iterationLength, isTypedArray) {
            var output = 0;
            output += incomingObject.length;

            if (isTypedArray) {
                for (var i = 0; i < iterationLength; ++i) {
                    output += incomingObject[i]|0;
                }
            } else {
                for (var i = 0; i < iterationLength; ++i) {
                    output += incomingObject[i]|0;
                }
            }
            return output;
        }
        noInline(${ arrayType }AndObjectSpeculationOutOfBounds);

        for (var i = 0; i < 1e4; ++i) {
            var output = ${ arrayType }AndObjectSpeculationOutOfBounds(typedArray, 128, true);
            if (output !== 32 * 65)
                throw "${ arrayType }AndObjectSpeculationOutOfBounds(typedArray, 128, true) failed, value = " + output;

            var output = ${ arrayType }AndObjectSpeculationOutOfBounds(regularArray, 128, false);
            if (output !== 32 * 65)
                throw "${ arrayType }AndObjectSpeculationOutOfBounds(regularArray, 128, false) failed, value = " + output;
        }`
    eval(testCode);
}

testArray("Int8Array");
testArray("Uint8Array");
testArray("Uint8ClampedArray");
testArray("Int16Array");
testArray("Uint16Array");
testArray("Int32Array");
testArray("Uint32Array");
testArray("Float32Array");
testArray("Float64Array");
