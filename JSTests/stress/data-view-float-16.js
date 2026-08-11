//@ runDefault("--useConcurrentJIT=0")
for (let i = 0; i < 2; i++) {
    new DataView(new ArrayBuffer(2)).setFloat16(0, 1);
}
for (let j = 0; j < 99999; j++) { }

