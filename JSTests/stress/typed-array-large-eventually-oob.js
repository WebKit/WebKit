//@ skip if $memoryLimited or ($architecture != "arm64" && $architecture != "x86_64")
//@ runDefault()
function getArrayLength(array)
{
    return array.length;
}
noInline(getArrayLength);
function getByVal(array, index)
{
    return array[index];
}
noInline(getByVal);
function putByVal(array, index, value)
{
    'use strict';
    array[index] = value;
}
noInline(putByVal);

let oneGiga = 1024 * 1024 * 1024;

function test(array, actualLength, string)
{
    for (var i = 0; i < 100000; ++i) {
        var l = getArrayLength(array);
        if (l != actualLength)
            throw ("Wrong array length: " + l + " instead of the expected " + actualLength + " in case " + string);
        var index = i;
        var value = i % 100;
        putByVal(array, index, value);
        var result = getByVal(array, index);
        if (result != value)
            throw ("Expected " + value + " but got " + result + " in case " + string);
    }
    var value = 42;
    var index = actualLength + 10;
    putByVal(array, index, value);
    var result = getByVal(array, index);
    if (result != undefined)
        throw ("In out-of-bounds case, expected undefined but got " + result + " in case " + string);
}

let threeGigs = 3 * oneGiga;
let fourGigs = 4 * oneGiga;

test(new Int8Array(threeGigs), threeGigs, "Int8Array/3GB");
test(new Uint8Array(fourGigs), fourGigs, "Uint8Array/4GB");
test(new Uint8ClampedArray(threeGigs), threeGigs, "Uint8ClampedArray/3GB");
