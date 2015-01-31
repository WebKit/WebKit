description(
"This tests Abstract Relatioal Comparison results with Symbols."
);

var symbol = Symbol("Cocoa");
// Test Abstract Relational Comparison.
var relationalOperators = [
    "<", "<=", ">", ">="
];
var object = {};
var array = [];
var date = new Date();

relationalOperators.forEach(function (op) {
    var targets = [
        "42",
        "NaN",
        "Infinity",
        "true",
        "false",
        "null",
        "undefined",
        "'Cappuccino'",
        "symbol",
        "Symbol.iterator",
        "object",
        "array",
        "date",
    ];

    targets.forEach(function (target) {
        shouldThrow(target + " " + op + " symbol", "'TypeError: Type error'");
        shouldThrow("symbol " + op + " " + target, "'TypeError: Type error'");
    });
});

successfullyParsed = true;
