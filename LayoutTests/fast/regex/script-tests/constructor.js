description("This test checks use of the regexp constructor.");

var re = /abc/;

shouldBeTrue("re === RegExp(re)");
shouldBeTrue("re !== new RegExp(re)");
shouldThrow("re === RegExp(re,'i')");
shouldThrow("re !== new RegExp(re,'i')");

