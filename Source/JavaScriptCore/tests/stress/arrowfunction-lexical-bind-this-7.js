var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var deepScope = function (x, y) {
    var _x = x, _y = y;
    return ()=> _x + _y + this.val;
};

var a = deepScope.call({val:'A'}, 'D', 'E');
var b = deepScope.call({val:'B'}, 'D', 'F');
var c = deepScope.call({val:'C'}, 'D', 'G');

var anotherScope = function (_af) {
    return _af();
};

for (var i = 0; i < 1000; i++) {
    testCase(c(), anotherScope.call({val:'I'}, c), "Error: this is not lexically binded inside of the arrow function #1");
    testCase(b(), anotherScope.call({val:'J'}, b), "Error: this is not lexically binded inside of the arrow function #2");
    testCase(a(), anotherScope.call({val:'K'}, a), "Error: this is not lexically binded inside of the arrow function #3");
}
