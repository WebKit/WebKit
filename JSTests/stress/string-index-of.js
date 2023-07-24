function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test0(string) {
    return string.indexOf("Hello");
}
noInline(test0);

function test1(string) {
    return string.indexOf("okHellooko");
}
noInline(test1);

function test2(string, index) {
    return string.indexOf("Hello", index);
}

function test3(string, index) {
    return string.indexOf("okHellooko", index);
}

function test4(string) {
    return string.indexOf("H");
}

function test5(string, index) {
    return string.indexOf("H", index);
}

var string = ".............................................okokHellookok................................";
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test0(string), 49);
    shouldBe(test1(string), 47);

    shouldBe(test2(string, 20), 49);
    shouldBe(test2(string, 47), 49);
    shouldBe(test2(string, 49), 49);
    shouldBe(test2(string, 50), -1);
    shouldBe(test2(string, string.length), -1);
    shouldBe(test3(string, 20), 47);
    shouldBe(test3(string, 47), 47);
    shouldBe(test3(string, 48), -1);
    shouldBe(test3(string, string.length), -1);

    shouldBe(test4(string), 49);
    shouldBe(test5(string, 20), 49);
    shouldBe(test5(string, 49), 49);
    shouldBe(test5(string, 50), -1);
}
