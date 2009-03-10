description(
"This test makes sure we don't incorrectly cache virtual registers in system registers across VM branches"
);

shouldBeTrue("(true ? function(){ return true; } : a.b)()");
shouldBeTrue("(function(){ return true; } || a.b)()");
shouldBeTrue("(function(){ return (true ? function(){return true;} : a.b)(); })()");
shouldBeTrue("(function(){ return (function(){return true;} || a.b)(); })()");
shouldBeTrue("(function(){ var i = 0; var result = false; var a = {c:true}.c; do { result = a; i++; } while (i < 2); return result; })()");

var successfullyParsed = true;
