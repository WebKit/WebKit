//@ $skipModes << :lockdown if $memoryLimited
// Here we're using $memoryLimited as a proxy for "slower device" and this test is very slow in lockdown.

var a1 = new Array(1024 * 1024 * 1);
a1.fill(99);
for (var i = 0; i < 1e2; ++i)
    var a2 = Uint8Array.from(a1);
