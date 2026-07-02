description("Make sure we do a correct property resolution of a global object property when contained by eval.");

var pass = false;

var accessGlobal = (function() { return eval("var pass=true; (function(){ return pass; })"); })();
var accessLocal = (function() { var pass = false; return (function() { return eval("var pass=true; (function(){ return pass; })"); })(); })();

shouldBeTrue("accessGlobal()");
shouldBeTrue("accessLocal()");
