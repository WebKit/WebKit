description(
'This test checks for regression against <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=6234">6234: Can delete array index property incorrectly.</a>'
);

var a = new Array();  
a[1]     = "before";  
a['1.0'] = "after";  
delete a['1.0'];  
shouldBe("a[1]", '"before"');

successfullyParsed = true;
