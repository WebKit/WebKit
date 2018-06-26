// This test should not crash.

for (var i = 0; i < 10000; i++) {
    var arr = [];
    arr.constructor = {
        [Symbol.species]: function () { return ['unmodifiable']; }
    }
    arr.concat([1]);
}
