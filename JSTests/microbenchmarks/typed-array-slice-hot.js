var typedArray = new Uint8Array(1024);
typedArray.fill(253);
var output = typedArray.slice();
for (let i = 0; i < 1000000; i++) {
    output = output.slice();
}
