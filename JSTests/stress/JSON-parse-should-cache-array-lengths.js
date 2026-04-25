// This test should not hang, and should call the reviver function the expected number of times.

function shouldBe(actualExpr, expectedExpr) {
    function toString(x) {
        return "" + x;
    }

    let actual = eval(actualExpr);
    let expected = eval(expectedExpr);
    if (typeof actual != typeof expected)
        throw Error("expected type " + typeof expected + " actual type " + typeof actual);
    if (toString(actual) != toString(expected))
        throw Error("expected: " + expected + " actual: " + actual);
}

let result;
let visited;

function test(parseString, clearLength) {
    visited = [];
    var result = JSON.parse(parseString, function (key, value) {
        visited.push('{' + key + ':' + value + '}');
        if (clearLength)
            this.length = 0;
        return; // returning undefined deletes the value.
    });
    return result;
}

result = test('[10]', false);
shouldBe("result", "undefined");
shouldBe("visited", "['{0:10}','{:}']");
shouldBe("visited.length", "2");

result = test('[10]', true);
shouldBe("result", "undefined");
shouldBe("visited", "['{0:10}','{:}']");
shouldBe("visited.length", "2");

result = test('[ [ 10, 11 ], 12, [13, 14, 15], 16, 17]', false);
shouldBe("result", "undefined");
shouldBe("visited", "['{0:10}','{1:11}','{0:,}','{1:12}','{0:13}','{1:14}','{2:15}','{2:,,}','{3:16}','{4:17}','{:,,,,}']");
shouldBe("visited.length", "11");

result = test('[ [ 10, 11 ], 12, [13, 14, 15], 16, 17]', true);
shouldBe("result", "undefined");
shouldBe("visited", "['{0:10}','{1:undefined}','{0:}','{1:undefined}','{2:undefined}','{3:undefined}','{4:undefined}','{:}']");
shouldBe("visited.length", "8");

result = test('[ { "a": [ 10, 11 ], "b": 12 } , [ 13, { "c": 14 }, 15], 16, 17]', false);
shouldBe("result", "undefined");
shouldBe("visited", "['{0:10}','{1:11}','{a:,}','{b:12}','{0:[object Object]}','{0:13}','{c:14}','{1:[object Object]}','{2:15}','{1:,,}','{2:16}','{3:17}','{:,,,}']");
shouldBe("visited.length", "13");

result = test('[ { "a": [ 10, 11 ], "b": 12 } , [ 13, { "c": 14 }, 15], 16, 17]', true);
shouldBe("result", "undefined");
shouldBe("visited", "['{0:10}','{1:undefined}','{a:}','{b:12}','{0:[object Object]}','{1:undefined}','{2:undefined}','{3:undefined}','{:}']");
shouldBe("visited.length", "9");


function test2(parseString, clearLength) {
    visited = [];
    var result = JSON.parse(parseString, function (key, value) {
        visited.push('{' + key + ':' + value + '}');
        if (clearLength)
            this.length = 0;
        return (typeof value === "number") ? value * 2 : value;
    });
    return result;
}

result = test2('[10]', false);
shouldBe("result", "[20]");
shouldBe("visited", "['{0:10}','{:20}']");
shouldBe("visited.length", "2");

result = test2('[10]', true);
shouldBe("result", "[20]");
shouldBe("visited", "['{0:10}','{:20}']");
shouldBe("visited.length", "2");

result = test2('[ [ 10, 11 ], 12, [13, 14, 15], 16, 17]', false);
shouldBe("result", "[20,22,24,26,28,30,32,34]");
shouldBe("visited", "['{0:10}','{1:11}','{0:20,22}','{1:12}','{0:13}','{1:14}','{2:15}','{2:26,28,30}','{3:16}','{4:17}','{:20,22,24,26,28,30,32,34}']");
shouldBe("visited.length", "11");

result = test2('[ [ 10, 11 ], 12, [13, 14, 15], 16, 17]', true);
shouldBe("result", "[]");
shouldBe("visited", "['{0:10}','{1:undefined}','{0:}','{1:undefined}','{2:undefined}','{3:undefined}','{4:undefined}','{:}']");
shouldBe("visited.length", "8");

result = test2('[ { "a": [ 10, 11 ], "b": 12 } , [ 13, { "c": 14 }, 15], 16, 17]', false);
shouldBe("result", "['[object Object]',26,'[object Object]',30,32,34]");
shouldBe("visited", "['{0:10}','{1:11}','{a:20,22}','{b:12}','{0:[object Object]}','{0:13}','{c:14}','{1:[object Object]}','{2:15}','{1:26,[object Object],30}','{2:16}','{3:17}','{:[object Object],26,[object Object],30,32,34}']");
shouldBe("visited.length", "13");

result = test2('[ { "a": [ 10, 11 ], "b": 12 } , [ 13, { "c": 14 }, 15], 16, 17]', true);
shouldBe("result", "[]");
shouldBe("visited", "['{0:10}','{1:undefined}','{a:}','{b:12}','{0:[object Object]}','{1:undefined}','{2:undefined}','{3:undefined}','{:}']");
shouldBe("visited.length", "9");
