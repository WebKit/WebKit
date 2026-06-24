function allocate(length) {
    return new Uint8Array(length);
}
noInline(allocate);

let sum = 0;
for (let i = 0; i < 200000; ++i) {
    let array = allocate(1000);
    sum += array[0] + array[999];
}
if (sum !== 0)
    throw new Error("bad: " + sum);
