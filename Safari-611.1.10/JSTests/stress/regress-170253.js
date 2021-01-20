// This test passes if it does not crash.

Array.prototype.__defineGetter__(1000, () => 0);

for (let i = 0; i < 0x1000; i++)
    new Array(0x10).fill([{}, {}, {}, {}]);

for (let i = 0; i < 0x1000; i++) {
    let x = {length: 0x10};
    x.__defineGetter__(0, () => gc());
    Array.prototype.splice.call(x, 0);
}
