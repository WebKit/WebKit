description(

'Test for <a href="http://bugs.webkit.org/show_bug.cgi?id=16129">bug 16129</a>: REGRESSION (r27761-r27811): malloc error while visiting http://mysit.es (crashes release build).'

);

shouldThrow('/^[\s{-.\[\]\(\)]$/');

var successfullyParsed = true;
