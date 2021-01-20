description('Tests for scoping of variables in ES6 class syntax');

var local = "FAIL";
function test() {
    var local = "PASS";
    class A {
        getLocal(x) { return local; }
    };
    return new A().getLocal();
}

shouldBe('test()', '"PASS"');

var successfullyParsed = true;
