const backing = new ArrayBuffer(0x80, {maxByteLength: 0x100});
const arr = new Int32Array(backing);

function main(x) {
    arr[0] = x;
}

for (let i = 0; i < testLoopCount; i++) {
    if (i % 2 == 0) {
        backing.resize(0x90);
    } else {
        backing.resize(0x80);
    }
    main(i + 1000000.0000001);
}

