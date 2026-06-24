function allocate() {
    return new Int32Array(512);
}
noInline(allocate);

let sum = 0;
for (let i = 0; i < 100000; ++i) {
    let array = allocate();
    sum += array[0] + array[511];
}
if (sum !== 0)
    throw new Error("bad: " + sum);
