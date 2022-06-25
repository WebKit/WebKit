description("Test regexp compiling to make sure it doens't crash like bug 16127");

shouldBeTrue('!!/\\)[;\s]+/');
shouldThrow('/[/');
shouldThrow('/[a/');
shouldThrow('/[-/');
shouldBeTrue('!!/(a)\1/');
shouldBeTrue('!!/(a)\1{1,3}/');

testPassed("No crashes, yay!")
