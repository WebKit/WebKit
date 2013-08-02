description(
"Check that the DFG can handle MakeRope on values that have side effects on [[ToPrimitive]]."
);

function f(a,b,c,d,e) { return "1"+a+b+c+d+e+"6"; }
var a=new String("2")
var b = 5;
var e = false;
var k={valueOf:function(){return 4;}, toString:function(){return {}}}
s=String

var testOutput ="";
// Run enough times to ensure that f is dfg compiled:
var i;
for (i = 0; i < 500; i++)
	testOutput += f(a,new s(3),k, b, e);
testOutput += f(a,new s(3),k, b, e);
testOutput = testOutput.replace(/12345false6/g, "");
shouldBe("testOutput.length", "0");

