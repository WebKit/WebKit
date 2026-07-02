// This test passes if it does not crash.

arr = new Array;
Object.defineProperty(arr, 18, ({writable: true, configurable: true}));
for (var i = 0; i < 3; ++i) {
    Array.prototype.push.apply(arr, ["", () => { }, {}]);
    Array.prototype.sort.apply(arr, [edenGC, []]);
}
