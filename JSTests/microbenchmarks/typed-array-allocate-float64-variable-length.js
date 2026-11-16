function allocate(length) {
    return new Float64Array(length);
}
noInline(allocate);

let sum = 0;
for (let i = 0; i < 50000; ++i) {
    let array = allocate(1000);
    sum += array[0] + array[999];
}
if (sum !== 0)
    throw new Error("bad: " + sum);
