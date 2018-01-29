// Test that we properly clobber untyped uses.  This test should throw or crash.

let val;

for (var i = 0; i < 100000; i++)
    val = 42;

for (let i = 0; i < 1e6; i++) {
    if (val != null && val == 2) {
        throw "Val should be 42, but is 2";
    }
}
