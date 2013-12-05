var array = new Int8Array(new ArrayBuffer(100));

function foo(i) {
    return array[i];
}

for (var i = 0; i < 100000; ++i) {
    if (foo(i % 100) != 0)
        throw "Error";
}
