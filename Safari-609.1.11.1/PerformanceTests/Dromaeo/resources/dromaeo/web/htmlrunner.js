var startTest = top.startTest || function(){};
var test = top.test || function(name, fn){ fn(); };
var endTest = top.endTest || function(){};
var prep = top.prep || function(fn){ fn(); };
