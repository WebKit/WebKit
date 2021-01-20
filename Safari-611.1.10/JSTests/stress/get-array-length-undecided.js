function test(array) {
    return array.length;
}
noInline(test);

let array = new Array(10);
for (let i = 0; i < 10000; i++) {
    if (test(array) !== 10)
        throw new Error("bad result");
}
