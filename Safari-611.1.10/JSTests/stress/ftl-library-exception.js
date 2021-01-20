function foo(d){
    return Date.prototype.getTimezoneOffset.call(d);
}

noInline(foo);

var x;
var count = 100000;
var z = 0;
for (var i = 0 ; i < count; i++){
    try { 
        var q = foo(i < count - 10 ? new Date() : "a");
        x = false;
        z = q;
    } catch (e) {
        x = true;
    }
}

if (!x)
    throw "bad result: "+ x;
