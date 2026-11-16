let ta = new Int32Array(100);
for (let i = 0; i < 100; i++) ta[i] = i;

ta.forEach((v, i) => {
    if (i === 0) {
        ta.buffer;

        for (let j = 1; j < 100; j++) {
            ta[j] = 0xDEAD;
        }
    } else {
        if (v !== 0xDEAD) {
            throw new Error("read stale value at index ", i);
        }
    }
});
