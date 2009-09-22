description(
"This test checks that implicit reentry to global code through a getter does not clobber the calling register file."
);

var testVar = "FAIL";
function testGlobalCode(test) {
    document.write("<script>"+test+"<"+"/script>");
}
var testObject = {
    get getterTest(){ testGlobalCode("var a, b, c, d; testVar = 'PASS';"); },
    set setterTest(){ testGlobalCode("var e, f, g, h; testVar = 'PASS';"); },
    toString: function() { testGlobalCode("var i, j, k, l; testVar = 'PASS';"); return ''; },
    valueOf: function() { testGlobalCode("var m, n, o, p; testVar = 'PASS';"); return 0; },
    toStringTest: function() { "" + this; },
    valueOfTest: function() { 0 * this; }
};

shouldBe("testObject.getterTest; testVar;", '"PASS"');
var testVar = "FAIL";
shouldBe("testObject.setterTest = 1; testVar;", '"PASS"');
var testVar = "FAIL";
shouldBe("testObject.toStringTest(); testVar;", '"PASS"');
var testVar = "FAIL";
shouldBe("testObject.valueOfTest(); testVar;", '"PASS"');

successfullyParsed = true;
