description(
'Test for &lt;<a href="rdar://problem/5666078">rdar://problem/5666078</a>&gt; Cookie parsing terminates at the first semicolon, ignoring quotes (<a href="https://bugs.webkit.org/show_bug.cgi?id=16699">16699</a>)'
);

clearAllCookies();

debug("Check that setting a cookie with a semi-colon in a duoble-quoted value works");
cookiesShouldBe('disorder="477beccb;richard";Version=1;Path=/', 'disorder="477beccb;richard"');
clearCookies();

successfullyParsed = true;
