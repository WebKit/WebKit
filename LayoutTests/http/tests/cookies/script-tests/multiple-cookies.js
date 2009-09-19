description(
"This test checks that mulitple cookies are correctly set."
);

clearAllCookies();

debug("Check setting several cookies without clearing.");
cookiesShouldBe("test=foobar;", "test=foobar");
cookiesShouldBe("test2=foobar;", "test=foobar; test2=foobar");
cookiesShouldBe("test3=foobar;", "test=foobar; test2=foobar; test3=foobar");
clearCookies();

successfullyParsed = true;
