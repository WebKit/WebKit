description("This test checks use of the regexp constructor.");

var re = /abc/;

shouldBeTrue("re === RegExp(re)");
shouldBeTrue("re !== new RegExp(re)");
shouldBeTrue("re === RegExp(re,'i')");
shouldBeTrue("re !== new RegExp(re,'i')");

