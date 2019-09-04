//@ skip if ["arm", "mips"].include?($architecture)
//@ slow!
//@ runDefault("--jitPolicyScale=0")

// This test should not crash.

script = `
    let a = new Int32Array(1);
    for (let i = 0; i < 1000; ++i)
        ~a.byteOffset;

    transferArrayBuffer(a.buffer);

    eval(a.byteOffset);
    let description = describe(a.byteOffset);
    if (description !== 'Int32: 0')
        print(description);
`;

const iterations = 1000;
for (let i = 0; i < iterations; i++)
    runString(script);
