function foo(a, b) {
    return a == b;
}

function bar(a, b) {
    if (a == b)
        return "yes";
    else
        return "no";
}

function baz(a, b) {
    if (a != b)
        return "no";
    else
        return "yes";
}

var o = {f:1};
var p = {f:2};
var q = {f:3};

var array1 = [o, p, q];
var array2 = [o, null];
var expecteds = [true,"yes","yes",false,"no","no",false,"no","no",false,"no","no",false,"no","no",false,"no","no"];

var expectedsIndex = 0;

function dostuff(result) {
    if (result == expecteds[expectedsIndex % expecteds.length]) {
        expectedsIndex++;
        return;
    }
    
    print("Bad result with a = " + a + ", b = " + b + ": wanted " + expecteds[expectedsIndex % expecteds.length] + " but got " + result);
    throw "Error";
}

for (var i = 0; i < 100000; ++i) {
    var a = array1[i % array1.length];
    var b = array2[i % array2.length];
    dostuff(foo(a, b));
    dostuff(bar(a, b));
    dostuff(baz(a, b));
}


