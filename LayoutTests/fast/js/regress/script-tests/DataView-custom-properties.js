var array = new Array(10000);

for (var i = 0; i < 100000; ++i) {
    var thingy = new DataView(new ArrayBuffer(1000));
    switch (i % 3) {
    case 0:
        break;
    case 1:
        thingy.f = 42;
        break;
    case 2:
        thingy[0] = 42;
        break;
    }
    array[i % array.length] = thingy;
}

for (var i = 0; i < array.length; ++i) {
    if (array[i].byteLength != 1000)
        throw "Error: bad length: " + array[i].byteLength;
    if (array[i].buffer.byteLength != 1000)
        throw "Error: bad buffer.byteLength: " + array[i].buffer.byteLength;
    switch (i % 3) {
    case 0:
        break;
    case 1:
        if (array[i].f != 42)
            throw "Error: bad field 'f': " + array[i].f;
        break;
    case 2:
        if (array[i][0] != 42)
            throw "Error: bad element 0: " + array[i][0];
        break;
    }
}
