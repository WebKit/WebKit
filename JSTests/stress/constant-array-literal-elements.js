function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected));
}

function shouldThrowSyntaxError(source) {
    try {
        eval(source);
    } catch (error) {
        if (error instanceof SyntaxError)
            return;
        throw error;
    }
    throw new Error("expected SyntaxError: " + source);
}

function collect(...args) {
    return args.map((value) => value === Infinity ? "Infinity" : value);
}

function Collect(...args) {
    this.args = args;
}

var fakeApply = { apply(...args) { return "fake:" + args.join(); } };
var fakeReflect = { construct(target, args) { return "fake:" + args.join(); } };

function test() {
    shouldBe([1, 2, 3], [1, 2, 3]);
    shouldBe([1,2,3,], [1, 2, 3]);
    shouldBe([1, 2 , 3 ], [1, 2, 3]);
    shouldBe([1,
2,
3], [1, 2, 3]);
    shouldBe([1,/* comment */2], [1, 2]);
    shouldBe([1.5,2,null], [1.5, 2, null]);
    shouldBe([1.0, 2], [1, 2]);
    shouldBe([0x10,1e3,null,0], [16, 1000, null, 0]);
    shouldBe([12345678901234567890, 1], [12345678901234567890, 1]);
    shouldBe([null], [null]);
    shouldBe([1,,2].length, 3);
    shouldBe([1,2,,].length, 3);
    shouldBe([1,-1,2], [1, -1, 2]);
    shouldBe([1,2,"a"], [1, 2, "a"]);
    let x = 5;
    shouldBe([1,2,x], [1, 2, 5]);
    shouldBe([1,2,...[3]], [1, 2, 3]);
    shouldBe([1,2,x,3,4], [1, 2, 5, 3, 4]);
    shouldBe([1.5,null,1.0,x,null,2.5], [1.5, null, 1, 5, null, 2.5]);
    shouldBe([1,2,,x], [1, 2, , 5]);
    shouldBe([1,2,,x].length, 4);
    shouldBe([1,2+3,4], [1, 5, 4]);
    shouldBe([1, 2, 3 + 4], [1, 2, 7]);
    shouldBe([1,2,...[3],4], [1, 2, 3, 4]);
    shouldBe([1,2,[3,4],5], [1, 2, [3, 4], 5]);
    shouldBe(collect(...[1,2,3]), [1, 2, 3]);
    shouldBe(new Collect(...[1,2]).args, [1, 2]);
    shouldBe(Reflect.construct(Collect, [x, "a", x + 1]).args, [5, "a", 6]);
    shouldBe(Reflect.construct(Collect, [1, x, 2]).args, [1, 5, 2]);
    shouldBe(Reflect.construct(Collect, [0x80000000, 1.0, null]).args, [2147483648, 1, null]);
    shouldBe(collect.apply(null, [0x80000000, 1.0, null]), [2147483648, 1, null]);
    shouldBe([[1,2],[3,4]], [[1, 2], [3, 4]]);
    shouldBe(Math.max.apply(null, [1,7,3]), 7);
    shouldBe(Reflect.construct(Array, [1,2,3]), [1, 2, 3]);
    shouldBe(collect.apply(null, [1, 2.5, null, 1e400]), [1, 2.5, null, "Infinity"]);
    shouldBe(collect.apply(null, [null]), [null]);
    shouldBe(Reflect.construct(Collect, [1, 2.5, null]).args, [1, 2.5, null]);
    shouldBe(Reflect.construct(Collect, [7], Collect).args, [7]);
    shouldBe(fakeApply.apply(null, [1, 2]), "fake:,1,2");
    shouldBe(fakeReflect.construct(Collect, [1, 2]), "fake:1,2");
    shouldBe(Math.max(...[1,9,3]), 9);

    let a = [1,2,3];
    a.push(4);
    shouldBe(a, [1, 2, 3, 4]);
    let b = [1,2,3];
    let c = [1,2,3];
    b[0] = 9;
    shouldBe(b, [9, 2, 3]);
    shouldBe(c, [1, 2, 3]);

    let big = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40.5,null];
    shouldBe(big.length, 41);
    shouldBe(big[39], 40.5);
    shouldBe(big[40], null);

    shouldBe(eval("[1, 010]"), [1, 8]);
    shouldThrowSyntaxError("[1,2] = 3");
    shouldThrowSyntaxError("[1,2");
    shouldThrowSyntaxError("[1,2,3");
    shouldThrowSyntaxError("'use strict'; [1, 010]");
    shouldThrowSyntaxError("[1, 2, 3 +]");
    shouldThrowSyntaxError("[1, 2, x");
    shouldThrowSyntaxError("[1, 2 3]");
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test();
