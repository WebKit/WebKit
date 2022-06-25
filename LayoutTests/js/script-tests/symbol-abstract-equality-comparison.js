description(
"This tests Abstract Equality Comparison results with Symbols."
);

var symbol = Symbol("Cocoa");
var symbolObject = Object(symbol);
// Test Abstract Relational Comparison.
var relationalOperators = [
    "==", "!="
];
var object = {};
var array = [];
var date = new Date();

function Pair(key, result) {
    return [ key, result ];
}

relationalOperators.forEach(function (op) {
    var targets = [
        Pair("42", false),
        Pair("NaN", false),
        Pair("Infinity", false),
        Pair("true", false),
        Pair("false", false),
        Pair("null", false),
        Pair("undefined", false),
        Pair("'Cappuccino'", false),
        Pair("symbol", true),
        Pair("Symbol.iterator", false),
        Pair("object", false),
        Pair("array", false),
        Pair("date", false),
        Pair("symbolObject", true),
        Pair("Symbol('Cocoa')", false),
    ];

    targets.forEach(function (pair) {
        var target = pair[0];
        var result = pair[1];
        if (op === "!=") {
            result = !result;
        }
        shouldBe(target + " " + op + " symbol", String(result));
        shouldBe("symbol " + op + " " + target, String(result));
    });
});

successfullyParsed = true;
