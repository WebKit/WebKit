noInline(Float32Array.prototype.subarray);
function createManySubs(howMany, a, b, c, d) {
    var storage = new Float32Array(howMany * 4);
    for (var k=0; k < howMany; ++k) {
        var r = storage.subarray(k * 4, (k + 1) * 4);
        r[0] = a; r[1] = b; r[2] = c; r[3] = d;

        // some action
        r[0] += 2.3; r[1] += 12; r[2] *= 3.14; r[3] -= 999.1;
    }
}

function go() {
    var subtt = [];

    const iterationCount = 25;
    const arrayCount = 20000;

    var a, b, c, d;

    for (var iter=0; iter < iterationCount; ++iter) {
        a = Math.random() * 10;
        b = Math.random() * 10;
        c = Math.random() * 10;
        d = Math.random() * 10;
        createManySubs(arrayCount, a, b, c, d);
    }

}

go();
