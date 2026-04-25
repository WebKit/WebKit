//@ requireOptions("--useDollarVM=false")

// This test should not crash.
Date.prototype.valueOf;
Math.abs;

Object.prototype.__defineGetter__(0, function () {});

class Test extends Array {}

for (let i = 0; i < 100; i++)
    new Test(1, 2, 3, -4, 5, 6, 7, 8, 9).splice();

gc();
