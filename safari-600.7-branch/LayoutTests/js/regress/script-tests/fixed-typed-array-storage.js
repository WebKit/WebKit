var array = new Int8Array(new ArrayBuffer(100));

function foo() {
    return array[0];
}

for (var i = 0; i < 100000; ++i) {
    if (foo() != 0)
        throw "Error";
}
