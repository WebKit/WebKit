//@ requireOptions("--jitPolicyScale=0")
function foo(a0, a1) {
    try {
        foo(a0, a0);
    } catch {}
    a1.length;
}

const xs = new Uint8Array(new ArrayBuffer(0, { maxByteLength: 0 }));

for (let i = 0; i < 10000; i++) {
    foo([], '');
    foo(xs, []);
}
