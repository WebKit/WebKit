function mathWithOutOfBoundsAccess(arrayA, arrayB)
{
    var output = 0;
    for (var i = 0; i < 10; ++i) {
        output += (arrayA[i] + arrayB[i]);
    }
    return output;
}

noInline(mathWithOutOfBoundsAccess);

function test() {
    var integerArray = [0, 1, 2, 3, 4];
    var doubleArray = [0.1, 1.2, 2.3, 3.4, 4.5];

    for (var i = 0; i < 1e5; ++i)
        mathWithOutOfBoundsAccess(integerArray, doubleArray);
}
test();