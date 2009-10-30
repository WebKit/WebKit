description(
"This tests that for/in statements behave correctly when cached."
);

function forIn1() {
    var result = [];
    var o = { x: 1 };
    for (var p in o)
        result.push(p);
    return result;
}
forIn1();
Object.prototype.y = 2;
shouldBe("forIn1()", "['x', 'y']");
delete Object.prototype.y;

function forIn2() {
    var result = [];
    var o = { x: 1, __proto__: null };
    for (var p in o)
        result.push(p);
    return result;
}
forIn2();
shouldBe("forIn2()", "['x']");

function forIn3(proto) {
    var result = [];
    var o = { x: 1, __proto__: proto };
    for (var p in o)
        result.push(p);
    return result;
}
forIn3({ __proto__: { y1: 2 } });
forIn3({ __proto__: { y1: 2 } });
shouldBe("forIn3({ __proto__: { y1: 2 } })", "['x', 'y1']");

forIn3({ y2 : 2, __proto__: null });
forIn3({ y2 : 2, __proto__: null });
shouldBe("forIn3({ y2 : 2, __proto__: null })", "['x', 'y2']");

forIn3({ __proto__: { __proto__: { y3 : 2 } } });
forIn3({ __proto__: { __proto__: { y3 : 2 } } });
shouldBe("forIn3({ __proto__: { __proto__: { y3 : 2 } } })", "['x', 'y3']");

function forIn4(o) {
    var result = [];
    for (var p in o)
        result.push(p);
    return result;
}
var objectWithArrayAsProto = {};
objectWithArrayAsProto.__proto__ = [];
shouldBe("forIn4(objectWithArrayAsProto)", "[]");
objectWithArrayAsProto.__proto__[0]=1;
shouldBe("forIn4(objectWithArrayAsProto)", "['0']");

function forIn5(o) {
    for (var i in o)
        return [i, o[i]];
}

shouldBe("forIn5({get foo() { return 'called getter'} })", "['foo', 'called getter']");
shouldBe("forIn5({set foo() { } })", "['foo', undefined]");
shouldBe("forIn5({get foo() { return 'called getter'}, set foo() { }})", "['foo', 'called getter']");


var successfullyParsed = true;
