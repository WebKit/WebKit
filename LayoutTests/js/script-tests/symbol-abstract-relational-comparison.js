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
        shouldThrow(target + " " + op + " symbol", `"TypeError: Cannot convert a symbol to a number"`);
        shouldThrow("symbol " + op + " " + target, `"TypeError: Cannot convert a symbol to a number"`);
    });
});

successfullyParsed = true;
