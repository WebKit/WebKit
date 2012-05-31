description(
"This test verifies finding local variable within an eval\'ed function (https://bugs.webkit.org/show_bug.cgi?id=87158)"
);

var globalA = 0;

function testEvalFindsLocal() {
    with ({ a: 1}) {
        ( function () { globalA = eval("var a = 2; ( function() { return a; })()"); })();
    }
}

testEvalFindsLocal();
shouldBe("globalA", "2");
