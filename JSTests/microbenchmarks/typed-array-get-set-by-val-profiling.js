//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ skip if $architecture == "x86"
//@ $skipModes << :lockdown if $buildType == "debug"

// The type of arrayObject is polymorphic, but the access we do on it are not.
function nonPolymorphicUint8ClampedArraySetter(arrayObject, isTypedArray) {
    if (isTypedArray) {
        for (var i = 0; i < arrayObject.length; ++i) {
            arrayObject[i] = i;
        }
    } else {
        for (var i = 0; i < arrayObject.length; ++i) {
            arrayObject[i] = i;
        }
    }
}
noInline(nonPolymorphicUint8ClampedArraySetter);

function nonPolymorphicFloat64ArraySetter(arrayObject, isTypedArray) {
    if (isTypedArray) {
        for (var i = 0; i < arrayObject.length; ++i) {
            arrayObject[i] = i + 0.5;
        }
    } else {
        for (var i = 0; i < arrayObject.length; ++i) {
            arrayObject[i] = i + 0.5;
        }
    }
}
noInline(nonPolymorphicFloat64ArraySetter);

function nonPolymorphicUint8ClampedArrayGetter(arrayObject, isTypedArray) {
    var output = 0;
    if (isTypedArray) {
        for (var i = 0; i < arrayObject.length; ++i) {
            output += arrayObject[i];
        }
    } else {
        for (var i = 0; i < arrayObject.length; ++i) {
            output += arrayObject[i];
        }
    }
    return output;
}
noInline(nonPolymorphicUint8ClampedArrayGetter);

function nonPolymorphicFloat64ArrayGetter(arrayObject, isTypedArray) {
    var output = 0;
    if (isTypedArray) {
        for (var i = 0; i < arrayObject.length; ++i) {
            output += arrayObject[i];
        }
    } else {
        for (var i = 0; i < arrayObject.length; ++i) {
            output += arrayObject[i];
        }
    }
    return output
}
noInline(nonPolymorphicFloat64ArrayGetter);

function test() {
    var uint8ClampedArray = new Uint8ClampedArray(1024);
    var float64Array = new Float64Array(1024);
    var regularArray = new Array(32);

    var output = 0;
    for (var i = 0; i < 5000; ++i) {
        nonPolymorphicUint8ClampedArraySetter(uint8ClampedArray, true);
        nonPolymorphicUint8ClampedArraySetter(regularArray, false);
        nonPolymorphicFloat64ArraySetter(float64Array, true);
        nonPolymorphicFloat64ArraySetter(regularArray, false);

        output += nonPolymorphicUint8ClampedArrayGetter(uint8ClampedArray, true);
        output += nonPolymorphicUint8ClampedArrayGetter(regularArray, false);
        output += nonPolymorphicFloat64ArrayGetter(float64Array, true);
        output += nonPolymorphicFloat64ArrayGetter(regularArray, false);
    }
    return output;
}

test();
