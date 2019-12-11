//@ skip
// This test should not crash.  Note: it takes about 14 minutes to run on a debug build.

C = class {};
for (var i = 0; i < 50000; ++i)
    C = class extends C {};
gc();

