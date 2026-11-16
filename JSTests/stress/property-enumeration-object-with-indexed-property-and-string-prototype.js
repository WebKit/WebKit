function assert(actual, expected) {
    if (actual.length !== expected.length)
        throw new Error(`Length mismatch: got [${actual}], expected [${expected}]`);

    for (let i = 0; i < expected.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error(`Mismatch at index ${i}: got "${actual[i]}", expected "${expected[i]}"`);
    }
}

function getKey(x) {
    var arr = [];
    for (var i in x) {
        arr.push(i);
    }
    return arr;
}

function opt() {
    var x = new String("ab");
    function B() {
        this[0] = 4;
        this.bar = 77;
    }
    B.prototype = x;
    var y = new B();
    return getKey(y);
}

for (let i = 0; i < testLoopCount; i++) {
    assert(opt(), ['0', 'bar', '1']);
}
