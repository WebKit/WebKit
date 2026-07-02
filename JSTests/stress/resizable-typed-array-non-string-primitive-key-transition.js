for (let i = 0; i < 1000; i++) {
    const buf = new ArrayBuffer(7, { maxByteLength: 3378 });
    const ta = new Int8Array(buf);
    ta[undefined] = 42;
    ta[0];
}

for (let i = 0; i < 1000; i++) {
    const buf = new ArrayBuffer(7, { maxByteLength: 3378 });
    const ta = new Int8Array(buf);
    ta[null] = 42;
    ta[0];
}

for (let i = 0; i < 1000; i++) {
    const buf = new ArrayBuffer(7, { maxByteLength: 3378 });
    const ta = new Int8Array(buf);
    ta[true] = 42;
    ta[0];
}

for (let i = 0; i < 1000; i++) {
    const buf = new ArrayBuffer(7, { maxByteLength: 3378 });
    const ta = new Int8Array(buf);
    ta[false] = 42;
    ta[0];
}
