load("./standalone-pre.js", "caller relative");

"use strict";

var typedArrays = ["Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Float32Array", "Float64Array"];

function forEachTypedArray(constructors, testFunction, name, args) {
    for (let i = 0; i < constructors.length; ++i) {
        let typedArray = constructors[i];

        let result;
        if (name !== "")
            result = eval(typedArray + "." + name + args)
        else
            result = eval("new " + typedArray + args)

        let testResult = testFunction(result, typedArray)
        if (testResult !== true)
            return testResult;
    }

    return true;
}

function hasSameValues(msg, array1, array2) {
    if (array1.length !== array2.length) {
        debug(msg +  "The arrays had differing lengths, first array: " + array1 + " length: " + array1.length + " second array: " + array2 + " length" + array2.length);
        return false;
    }

    let allSame = true;
    for (let i = 0; i < array1.length; ++i) {
        allSame = allSame && Object.is(array1[i], array2[i]);
    }

    if (!allSame)
        debug(msg +  "The array did not have all the expected elements, first array: " + array1 + " second array: " + array2);
    return allSame;

}

function testConstructorFunction(name, args, expected) {
    function foo(array, constructor) {
        if (!hasSameValues(constructor + "." + name + " did not produce the correct result on " + name + args, array, expected))
            return false
        return true;
    }

    return forEachTypedArray(typedArrays, foo, name, args);
}

function testConstructor(args, expected) {
    function foo(array, constructor) {
        if (!hasSameValues(constructor + args + " did not produce the correct result", array, expected))
            return false
        return true;
    }

    return forEachTypedArray(typedArrays, foo, "", args);
}
