function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testing(object) {
    shouldBe(object[0], 0);
    shouldBe(object[1], 1);
    shouldBe(object[2], "String");
}
noInline(testing);

function testing2(object) {
    shouldBe(object[0], 0);
    shouldBe(object[1], 1);
    shouldBe(object[2], "String");
}
noInline(testing2);

for (var i = 0; i < 10000; ++i)
    testing({
        0: 0,
        1: 1,
        2: "String"
    });

testing({
    0: 0,
    get 1() {
        return 1;
    },
    2: "String"
});

for (var i = 0; i < 10000; ++i)
    testing2({
        0: 0,
        get 1() {
            return 1;
        },
        2: "String"
    });

/* vim: set sw=4 ts=4 et tw=80 : */
