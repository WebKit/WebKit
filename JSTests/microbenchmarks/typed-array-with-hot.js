var typedArray = new Uint8Array(1024);
typedArray.fill(253);
var output = typedArray.with(0, 1);
for (let i = 0; i < 100000; i++) {
    output = output.with(i & 1023, i);
}
