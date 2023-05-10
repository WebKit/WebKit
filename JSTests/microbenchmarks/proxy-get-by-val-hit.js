(function() {
    var proxy = new Proxy({}, {});
    var keys = [ "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p" ];
    for (let key of keys)
        proxy[key] = 42;

    for (var i = 0; i < 1e7; ++i)
        proxy[keys[i & 15]];
})();
