function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual + " expected " + expected);
}

var counters = {};
function incrementProperty() {
    counters.count++;
    return counters.count + 1;
}
noInline(incrementProperty);

var globalCount;
function incrementGlobal() {
    globalCount++;
    return globalCount * 2;
}
noInline(incrementGlobal);

let lexicalCount;
function incrementLexical() {
    lexicalCount++;
    return lexicalCount * 3;
}
noInline(incrementLexical);

var incrementClosure = (function () {
    var closureCount;
    return function (reset) {
        if (reset !== undefined)
            closureCount = reset;
        closureCount++;
        return closureCount - 1;
    };
})();
noInline(incrementClosure);

var addConstantClosure = (function () {
    var constantCount = 0 / 0;
    return function () {
        return constantCount + 0.5;
    };
})();
noInline(addConstantClosure);

function addX(o) {
    return o.x + 0.5;
}
noInline(addX);

function addY(o) {
    return o.y + 0.5;
}
noInline(addY);

var yObjects = [{ y: NaN }, { z: 0, y: NaN }, { y: 1.5 }, { z: 0, y: 1 }];
var yResults = [NaN, NaN, 2, 1.5];

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(incrementProperty(), NaN);
    shouldBe(incrementGlobal(), NaN);
    shouldBe(incrementLexical(), NaN);
    shouldBe(incrementClosure(), NaN);
    shouldBe(addConstantClosure(), NaN);
    shouldBe(addX({ x: i % 3 ? 1.5 : NaN }), i % 3 ? 2 : NaN);
    shouldBe(addY(yObjects[i & 3]), yResults[i & 3]);
}

shouldBe(addX({ x: "a" }), "a0.5");
shouldBe(addX({ x: 1 }), 1.5);
shouldBe(addY({ z: 0, y: "b" }), "b0.5");
shouldBe(addY({ y: 2 }), 2.5);
counters.count = 1;
shouldBe(incrementProperty(), 3);
globalCount = 2;
shouldBe(incrementGlobal(), 6);
globalCount = "c";
shouldBe(incrementGlobal(), NaN);
lexicalCount = 5;
shouldBe(incrementLexical(), 18);
lexicalCount = "d";
shouldBe(incrementLexical(), NaN);
shouldBe(incrementClosure(4), 4);
shouldBe(incrementClosure("e"), NaN);
