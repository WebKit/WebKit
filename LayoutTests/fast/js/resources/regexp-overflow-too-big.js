description('Tests a regular expression that hits the recursion limit.');

var complexPattern = "";
for (var i = 0; i < 18; ++i)
    complexPattern += "a?";
for (var i = 0; i < 18; ++i)
    complexPattern += "a";
complexPattern = "\1(" + complexPattern + ")";

var complexInput = "";
for (var i = 0; i < 18; ++i)
    complexInput += "a";

shouldBe('new RegExp(complexPattern + complexPattern).exec(complexInput + complexInput)', 'null'); // Too big

var successfullyParsed = true;
