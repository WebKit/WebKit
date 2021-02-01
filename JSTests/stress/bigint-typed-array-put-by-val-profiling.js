function testArray(arrayType)
{
    var testCode =
        `
        function testOutOfBoundsValues(regularArray, typedArray) {
            for (var i = 0; i < 16; ++i) {
                var typedArrayValue = typedArray[i]
                if (typedArrayValue !== BigInt(i)) {
                    throw "Failed ${ arrayType }AndObjectSpeculationInBounds, typedArrayValue = " + typedArrayValue + " for i = " + i;
                }
                var regularArrayValue = regularArray[i];
                if (regularArrayValue !== BigInt(i)) {
                    throw "Failed ${ arrayType }AndObjectSpeculationInBounds, regularArrayValue = " + regularArrayValue + " for i = " + i;
                }
            }
            for (var i = 16; i < 24; ++i) {
                var typedArrayValue = typedArray[i]
                if (typedArrayValue !== undefined) {
                    throw "Failed ${ arrayType }AndObjectSpeculationInBounds, typedArrayValue = " + typedArrayValue + " for i = " + i;
                }
                var regularArrayValue = regularArray[i];
                if (regularArrayValue !== BigInt(i)) {
                    throw "Failed ${ arrayType }AndObjectSpeculationInBounds, regularArrayValue = " + regularArrayValue + " for i = " + i;
                }
            }
        }

        // We make this look like a polymorphic types for incomingObject but the GetByVal are never actually
        // polymorphic. The boolean isTypedArray let us differentiate the types.
        function ${ arrayType }AndObjectSpeculationInBounds(incomingObject, iterationLength, isTypedArray) {
            if (isTypedArray) {
                for (var i = 0; i < iterationLength; ++i) {
                    incomingObject[i] = BigInt(i);
                }
            } else {
                for (var i = 0; i < iterationLength; ++i) {
                    incomingObject[i] = BigInt(i);
                }
            }
        }
        noInline(${ arrayType }AndObjectSpeculationInBounds);

        var typedArray = new ${ arrayType }(16);
        var regularArray = new Array(16);

        // Access in bounds.
        for (var i = 0; i < 1e4; ++i) {
            ${ arrayType }AndObjectSpeculationInBounds(regularArray, 16, false);
            ${ arrayType }AndObjectSpeculationInBounds(typedArray, 16, true);
        }
        for (var i = 0; i < 16; ++i) {
            var typedArrayValue = typedArray[i]
            if (typedArrayValue !== BigInt(i)) {
                throw "Failed ${ arrayType }AndObjectSpeculationInBounds, typedArrayValue = " + typedArrayValue + " for i = " + i;
            }
            var regularArrayValue = regularArray[i];
            if (regularArrayValue !== BigInt(i)) {
                throw "Failed ${ arrayType }AndObjectSpeculationInBounds, regularArrayValue = " + regularArrayValue + " for i = " + i;
            }
        }

        // One "out of bounds" on top of the in bounds profile.
        ${ arrayType }AndObjectSpeculationInBounds(regularArray, 24, false);
        ${ arrayType }AndObjectSpeculationInBounds(typedArray, 24, true);
        testOutOfBoundsValues(regularArray, typedArray);

        // Same but here we make out-of-bounds a normal case.
        function ${ arrayType }AndObjectSpeculationOutOfBounds(incomingObject, iterationLength, isTypedArray) {
            if (isTypedArray) {
                for (var i = 0; i < iterationLength; ++i) {
                    incomingObject[i] = BigInt(i);
                }
            } else {
                for (var i = 0; i < iterationLength; ++i) {
                    incomingObject[i] = BigInt(i);
                }
            }
        }
        noInline(${ arrayType }AndObjectSpeculationOutOfBounds);

        var typedArray = new ${ arrayType }(16);
        var regularArray = new Array(16);
        for (var i = 0; i < 1e4; ++i) {
            ${ arrayType }AndObjectSpeculationInBounds(regularArray, 24, false);
            ${ arrayType }AndObjectSpeculationInBounds(typedArray, 24, true);
        }`
    eval(testCode);
}

testArray("BigInt64Array");
testArray("BigUint64Array");
