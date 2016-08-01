function Test(value, returnIt) {
    this.value = value;
    this.returnIt = returnIt;
}

var tests = [
    new Test("string", false),
    new Test(5, false),
    new Test(6.5, false),
    new Test(void(0), false),
    new Test(null, false),
    new Test(true, false),
    new Test(false, false),
    new Test(Symbol.iterator, false),
    new Test({f:42}, true),
    new Test([1, 2, 3], true),
    new Test(new String("string"), true),
    new Test(new Number(42), true),
    new Test(new Boolean(false), true),
    new Test(new Boolean(false), true),
    new Test(Object(Symbol.iterator), true),
];

tests.forEach(function (test) {
    function Constructor() {
        return test.value;
    }

    var result = new Constructor();
    if (test.returnIt) {
        if (test.value !== result) {
            throw "Bad result: " + result;
        }
    } else {
        if (!(result instanceof Constructor)) {
            throw "Bad result: " + result;
        }
    }
});
