var proxy = new Proxy({}, {});

var keys = [];

for (var i = 0; i < 0x100; ++i) {
    keys.push('key' + i);
    keys[i][0];
}

function test(proxy, key) {
    return proxy[key];
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var key = keys[i & (0x100 - 1)];
    test(proxy, key);
}
