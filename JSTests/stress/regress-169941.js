// This test should not crash.

function gc() {
    for (let i = 0; i < 0x10; i++) {
        var a2 = new ArrayBuffer(0x1000000);
    }
}

Array.prototype.__defineGetter__(0x1000, () => 1);

gc();

for (let i = 0; i < 0x100; i++) {
    var a1 = new Array(0x100).fill(1234.5678);
}

gc();

new Array(0x100).splice(0).toString();
