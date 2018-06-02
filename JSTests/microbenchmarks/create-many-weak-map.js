var root = [];

function create()
{
    var weakMap = new WeakMap();
    for (var i = 0; i < 1e2; ++i) {
        var key = {};
        var value = {};
        weakMap.set(key, value);
        if (i % 10 === 0)
            root.push(key);
    }
}

for (var i = 0; i < 1e2; ++i) {
    for (var j = 0; j < 1e2; ++j) {
        var map = create();
        root.push(map);
    }
}
