description("Test that we can handle excessively large initializer lists");

var initializerTestString = "var a=0";
for (var i = 0; i < 50000; i++)
    initializerTestString += ",a"+i+"="+i;
initializerTestString += ";return true;";

var declarationTestString = "var a";
for (var i = 0; i < 50000; i++)
    declarationTestString += ",a"+i;
declarationTestString += ";return true;";

var commaExpressionTestString = "1";
for (var i = 0; i < 50000; i++)
    commaExpressionTestString += ",1";
commaExpressionTestString += ";return true;";

shouldBeTrue("new Function(initializerTestString)()");
shouldBeTrue("new Function(declarationTestString)()");
shouldBeTrue("new Function(commaExpressionTestString)()");

successfullyParsed = true;
