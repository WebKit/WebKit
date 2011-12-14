description(
'<p>This test checks for a regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=3743">https://bugs.webkit.org/show_bug.cgi?id=3743</a>: Incorrect error message given for certain calls.</p><p>The test confirms that the correct exception is thrown in the event of a stack overflow during a call to Array.toString.</p><p>It is possible that this may need to be updated if WebKit gets an improvement to its JavaScript stack support.  Either through increasing the depth of the recursion, or through some other mechanism.</p>'
);

var ary=[0];
for(var i=1; i<10000; i++)
  ary=[ary, i];

shouldThrow("ary.toString()");

var successfullyParsed = true;
