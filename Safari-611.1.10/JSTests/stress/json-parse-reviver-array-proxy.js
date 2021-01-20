function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const json = '{"a": 1, "b": 2}';

for (let i = 1; i < 10000; i++) {
    let keys = [];
    let proxy = new Proxy([2, 3], {
        get: function(target, key) {
            keys.push(key);
            return target[key];
        },
        ownKeys: function() {
            throw new Error('[[OwnPropertyKeys]] should not be called');
        },
    });

    let result = JSON.parse(json, function(key, value) {
        if (key === 'a')
            this.b = proxy;
        return value;
    });

    shouldBe(keys.toString(), 'length,0,1');
    shouldBe(JSON.stringify(result), '{"a":1,"b":[2,3]}');
}
