description(
"This test checks that cookies are correctly set using Expires."
);

clearAllCookies();

debug("Check that setting a simple cookie works.");
var date = new Date();
date.setTime(date.getTime() + 60 * 1000);
cookiesShouldBe("test=foobar; Expires=" + date.toGMTString(), "test=foobar");
clearCookies();

debug("Check setting a cookie that timed out.");
date.setTime(date.getTime() - 2 * 60 * 1000);
cookiesShouldBe("test2=foobar; Expires=" + date.toGMTString(), "");
clearCookies();

successfullyParsed = true;
