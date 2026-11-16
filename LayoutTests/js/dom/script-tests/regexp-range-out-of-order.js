description(

'Test for <a href="http://bugs.webkit.org/show_bug.cgi?id=16129">bug 16129</a>: REGRESSION (r27761-r27811): malloc error while visiting http://mysit.es (crashes release build).'

);

shouldThrow('/^[\s{-.\[\]\(\)]$/', '"SyntaxError: Invalid regular expression: range out of order in character class"');
shouldThrow('new RegExp("[\u0101-\u0100]")', '"SyntaxError: Invalid regular expression: range out of order in character class"');
