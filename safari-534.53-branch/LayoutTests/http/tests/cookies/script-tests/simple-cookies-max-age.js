description(
"This test checks that cookies are correctly set using Max-Age."
);

clearAllCookies();

debug("Check that setting a simple cookie works.");
cookiesShouldBe("test=foobar; Max-Age=90000000", "test=foobar");
clearCookies();

debug("Check setting a cookie that timed out.");
cookiesShouldBe("test2=foobar; Max-Age=0", "");
clearCookies();

successfullyParsed = true;
