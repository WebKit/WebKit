//@ skip if $architecture == "arm" or $memoryLimited

function foo() {
    let m = new WebAssembly.Memory({initial: 1000});
    try {
        foo();
    } catch {}
    for (let i = 0; i < 1000; i++) {
        new Uint8Array(i);
    }
}

try {
    foo();
} catch {}

try {
    eval('class C{0x1n');
} catch (e) {
    if (!(e instanceof SyntaxError))
        throw e;
}
