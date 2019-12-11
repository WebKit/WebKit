//@ requireOptions("--jitPolicyScale=0.1")

function foo(a) {
    a.length;
}

for (let i = 0; i < 100; i++) {
    let a = i % 2 ? new Uint8Array() : new Uint16Array();

    for (let j = 0; j < 2; j++)
        foo(a);

    gc();
    foo('');
    foo(a);
}
