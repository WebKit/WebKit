description("Regression test for https://bugs.webkit.org/show_bug.cgi?id=150372.");

// This test should not crash.
var re = /.*(?:(?:(?:(?:(?:(?:)))))).*/;
re.exec("hello");

testPassed("Did not crash.");
