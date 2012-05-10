description(
'Test for &lt;<a href="https://bugs.webkit.org/show_bug.cgi?id=85775">https://bugs.webkit.org/show_bug.cgi?id=85775</a>&gt; [BlackBerry] Cookie parsing issue. If the cookie value provided was (") then the browser creates a session cookie instead.'
);

clearAllCookies();

debug("Check that setting a cookie with a single-quoted value works.");
cookiesShouldBe('singleQuoted="value;Version=1;Path=/', 'singleQuoted="value');
clearCookies();

successfullyParsed = true;
