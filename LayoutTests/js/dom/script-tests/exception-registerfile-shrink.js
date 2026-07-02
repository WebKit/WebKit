description(
"Test for <a href='https://bugs.webkit.org/show_bug.cgi?id=41948'>REGRESSION(r60392): Registerfile can be unwound too far following an exception</a>. If the test doesn't crash, you pass."
);

eval('try { throw 0; } catch(e) {}');

var x = new String();
'' + escape(x.substring(0, 1));
