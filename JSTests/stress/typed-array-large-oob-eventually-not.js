//@ skip
// This tests takes >4s even in release mode on an M1 MBP, so I'd rather avoid running it on EWS by default.

let oneGiga = 1024 * 1024 * 1024;

function test(array, actualLength, string)
{
    for (var i = 0; i < 1000000; ++i) {
        var index = actualLength + 10;
        var value = 42;
        array[index] = value;
        var result = array[index];
        if (result != undefined)
            throw ("Expected " + value + " but got " + result + " in case " + string);
    }
    var value = 42;
    var index = 10;
    array[index] = value;
    var result = array[index]
    if (result != value)
        throw ("In out-of-bounds case, expected undefined but got " + result + " in case " + string);
}

let threeGigs = 3 * oneGiga;
let fourGigs = 4 * oneGiga;

test(new Int8Array(threeGigs), threeGigs, "Int8Array/3GB");
test(new Uint8Array(fourGigs), fourGigs, "Uint8Array/4GB");
test(new Uint8ClampedArray(threeGigs), threeGigs, "Uint8ClampedArray/3GB");
