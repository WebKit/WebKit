description(
'Test for &lt;<a href="https://bugs.webkit.org/show_bug.cgi?id=86067">https://bugs.webkit.org/show_bug.cgi?id=86067</a>&gt; [BlackBerry] Possible to clobber httpOnly cookie.'
);

debug("Check that we can't get or set httpOnly Cookies by JavaScript.");

if (document.cookie == "httpOnlyCookie=value")
    testFailed("We shouldn't get httpOnly cookies by JavaScript.");
else
    testPassed("We can't get httpOnly cookies by JavaScript.");

document.cookie = "httpOnlyCookie=changedValue";
if (document.cookie == "httpOnlyCookie=changedValue")
    testFailed("We shouldn't set httpOnly cookies by JavaScript.");
else
    testPassed("We can't set httpOnly cookies by JavaScript.");

clearCookies();

successfullyParsed = true;
