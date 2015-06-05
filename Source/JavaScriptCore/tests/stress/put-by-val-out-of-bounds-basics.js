// Put early out-of-bound data.
function opaquePutByValOnInt32ArrayEarlyOutOfBounds(array, index, value)
{
    array[index] = value;
}
noInline(opaquePutByValOnInt32ArrayEarlyOutOfBounds);

function testInt32ArrayEarlyOutOfBounds()
{
    // Warm up with an immediate out of bounds.
    var int32Array = new Array(10);
    for (var i = 0; i < 10; ++i) {
        opaquePutByValOnInt32ArrayEarlyOutOfBounds(int32Array, i, i);
        var value = int32Array[i];
        if (value !== i)
            throw "Failed opaquePutByValOnInt32ArrayEarlyOutOfBounds(int32Array, i, i) warmup with i = " + i + " value = " + value;
    }
    opaquePutByValOnInt32ArrayEarlyOutOfBounds(int32Array, 1042, 1);
    var value = int32Array[1042];
    if (value !== 1)
        throw "Failed opaquePutByValOnInt32ArrayEarlyOutOfBounds(int32Array, 1042, 1) value = " + value;

    var length = int32Array.length;
    if (int32Array.length !== 1043)
        throw "Incorrect int32Array.length, length = " + length;


    // We then do plenty of in-bounds accesses.
    for (var i = 0; i < 1e4; ++i) {
        for (var j = 0; j < 10; ++j) {
            opaquePutByValOnInt32ArrayEarlyOutOfBounds(int32Array, j, i);
            var value = int32Array[j];
            if (value !== i)
                throw "Failed opaquePutByValOnInt32ArrayEarlyOutOfBounds(int32Array, j, i) in-bounds with i = " + i + " j = " + j + " value = " + value;
        }
    }
}
testInt32ArrayEarlyOutOfBounds();


// Get out-of-bound data after a thousand run.
function opaquePutByValOnStringArrayHotOutOfBounds(array, index, value)
{
    array[index] = value;
}
noInline(opaquePutByValOnStringArrayHotOutOfBounds);

function testStringArrayHotOutOfBounds()
{
    // Warm up with in bounds access.
    var stringArray = new Array(10);
    for (var i = 0; i < 1e2; ++i) {
        for (var j = 0; j < 10; ++j) {
            opaquePutByValOnStringArrayHotOutOfBounds(stringArray, j, "" + i);
            var value = stringArray[j];
            if (value !== "" + i)
                throw "Failed opaquePutByValOnStringArrayHotOutOfBounds(stringArray, j, i) in-bounds with i = " + i + " j = " + j + " value = " + value;
        }
    }

    // Do a single out of bounds after warmup.
    opaquePutByValOnStringArrayHotOutOfBounds(stringArray, 513, 42);
    var value = stringArray[513];
    if (value !== 42)
        throw "Failed opaquePutByValOnStringArrayHotOutOfBounds(stringArray, 513, 42), value = " + value;

    // We then do plenty of in-bounds accesses.
    for (var i = 0; i < 1e3; ++i) {
        for (var j = 0; j < 10; ++j) {
            opaquePutByValOnStringArrayHotOutOfBounds(stringArray, j, "" + i);
            var value = stringArray[j];
            if (value !== "" + i)
                throw "Failed opaquePutByValOnStringArrayHotOutOfBounds(stringArray, j, i) in-bounds with i = " + i + " j = " + j + " value = " + value;
        }
    }

    // Followed by plenty of out-of-bounds accesses.
    for (var j = 514; j <= 1025; ++j)
        opaquePutByValOnStringArrayHotOutOfBounds(stringArray, j, "" + j);

    for (var j = 514; j <= 1025; ++j) {
        var value = stringArray[j];
        if (value !== "" + j)
            throw "Failed opaquePutByValOnStringArrayHotOutOfBounds(stringArray, j, j) in-bounds with j = " + j + " value = " + value;
    }
}
testStringArrayHotOutOfBounds();
