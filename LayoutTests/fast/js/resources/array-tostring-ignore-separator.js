description(
'This test checks for regressions against <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=6261">6261:Do not use a separator argument when doing toString/toLocalString.</a>'
);

var a = new Array;  
a[0] = 5;  
a[1] = 3;  
//Shouldn't use argument for toString  
shouldBe("a.toString('!')", "'5,3'");

successfullyParsed = true;
