function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = new Array(1024);
for (let i = 0; i < array.length; i++) {
    if (i % 3 === 0) array[i] = i;
    else if (i % 3 === 1) array[i] = `str${i}`;
    else array[i] = i * 1.1;
}
array[256] = [true, false, null, undefined, {}];
array[512] = [42, "hello", 3.14, BigInt(100), Symbol.for("test")];
array[768] = [[], {a: 1}, new Date(), /regex/, function() {}];

let r;
for (let i = 0; i < 1e5; i++) {
    r = array.flat();
}

shouldBe(r.length, 1021 + 5 + 5 + 5);
