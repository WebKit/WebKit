description(
"Tests that typedArray.set(otherTypedArray) does the right conversions when dealing with different types."
);

function MyRandom(z, w) {
    this.z = (z == null ? 42 : z);
    this.w = (w == null ? 23 : w);
}

MyRandom.prototype.get = function() {
    this.z = (36969 * (this.z & 65535) + (this.z >> 16)) | 0;
    this.w = (18000 * (this.w & 65535) + (this.w >> 16)) | 0;
    return ((this.z << 16) + this.w) | 0;
};

function testArraySet(fromType, toType) {
    // This function tests type conversions of set() by asserting that they are
    // equivalent to converting via JSValue.
    
    var rand = new MyRandom();
    
    function createIntArray() {
        var numSequences = 3;
        var numPerSequence = 4;
        var masks = [255, 65535, -1];
        
        var array = new fromType(numSequences * numPerSequence);
        for (var i = 0; i < numSequences; ++i) {
            for (var j = 0; j < numPerSequence; ++j)
                array[i * numPerSequence + j] = rand.get() & masks[i];
        }
        
        return array;
    }
    
    function createFloatArray() {
        var array = [0, 0/-1, 1, 42, 1.5, -2.5, 1/0, -1/0, 0/0];
        for (var i = 3; i--;)
            array.push(rand.get());
        
        return new fromType(array);
    }
    
    var sourceArray;
    if (fromType == Float32Array || fromType == Float64Array)
        sourceArray = createFloatArray();
    else
        sourceArray = createIntArray();
    
    function reference() {
        var array = new toType(sourceArray.length);
        for (var i = 0; i < sourceArray.length; ++i)
            array[i] = sourceArray[i];
        return array;
    }
    
    function usingConstruct() {
        return new toType(sourceArray);
    }
    
    function usingSet() {
        var array = new toType(sourceArray.length);
        array.set(sourceArray);
        return array;
    }
    
    var referenceArray = reference();
    
    var actualConstructArray = usingConstruct();
    
    if (isResultCorrect(referenceArray, actualConstructArray))
        testPassed("Copying " + stringify(sourceArray) + " to an array of type " + toType.name + " using the constructor resulted in " + stringify(actualConstructArray));
    else
        testFailed("Copying " + stringify(sourceArray) + " to an array of type " + toType.name + " using the constructor should have resulted in " + stringify(referenceArray) + " but instead resulted in " + stringify(actualConstructArray));
    
    var actualSetArray = usingSet();

    if (isResultCorrect(referenceArray, actualSetArray))
        testPassed("Copying " + stringify(sourceArray) + " to an array of type " + toType.name + " using typedArray.set() resulted in " + stringify(actualSetArray));
    else
        testFailed("Copying " + stringify(sourceArray) + " to an array of type " + toType.name + " using typedArray.set() should have resulted in " + stringify(referenceArray) + " but instead resulted in " + stringify(actualSetArray));
}

var types = [Int8Array, Int16Array, Int32Array, Uint8Array, Uint8ClampedArray, Uint16Array, Uint32Array, Float32Array, Float64Array];

for (var i = 0; i < types.length; ++i) {
    for (var j = 0; j < types.length; ++j)
        testArraySet(types[i], types[j]);
}
