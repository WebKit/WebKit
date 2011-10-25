description(
'This is a test case for <a href="https://bugs.webkit.org/show_bug.cgi?id=55346">bug 55346</a>.'
);

shouldBe("({}).toString.call(Error.prototype)", '"[object Error]"');
shouldBe("({}).toString.call(RangeError.prototype)", '"[object Error]"');
