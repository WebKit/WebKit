description(
"Tests what happens when you do ToString twice, and it has a side effect that clobbers the toString method in between the two ToStrings."
);

function foo(s, sideEffect) {
    var a = String(s);
    sideEffect(s);
    var b = String(s);
    return a + b;
}

var count = 0;
for (var i = 0; i < 200; ++i) {
    var code = "(function(s) { " + (i < 150 ? "return " + i + ";" : "count++; debug(\"hi!\"); s.toString = function() { return " + i + "; };") + " })";
    var sideEffect = eval(code);
    shouldBe("foo(new String(\"hello\"), sideEffect)", i < 150 ? "\"hellohello\"" : "\"hello" + i + "\"");
}
