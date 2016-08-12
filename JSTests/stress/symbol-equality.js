function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function equal(a, b) {
    return a == b;
}
noInline(equal);

function strictEqual(a, b) {
    return a === b;
}
noInline(strictEqual);

var s1 = Symbol()
var s2 = Symbol();

var list = [
    [ [ s1, s1 ], true ],
    [ [ s2, s1 ], false ],
    [ [ s1, s2 ], false ],
    [ [ s2, s2 ], true ],
    [ [ s2, 42 ], false ],
];

list.forEach(function (set) {
    var pair =  set[0];
    var result = set[1];
    for (var i = 0; i < 10000; ++i) {
        shouldBe(equal(pair[0], pair[1]), result);
        shouldBe(strictEqual(pair[0], pair[1]), result);
    }
});
