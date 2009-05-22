description("Test to make sure cached lookups on the global object are performed correctly.");

var funcNames = [];
var cachedFunctions = [];

for (var i in this) {
    funcNames.push(i);
    cachedFunctions.push(new Function("return " + i));
}

for (var i = 0; i < funcNames.length; i++) {
    shouldBe("cachedFunctions["+i+"]()", funcNames[i]);
    shouldBe("cachedFunctions["+i+"]()", funcNames[i]);
}

successfullyParsed = true;
