function foo(d){
    return Date.prototype.getTimezoneOffset.call(d);
}

noInline(foo);

var x;
var count = 100000;
for (var i = 0 ; i < count; i++){
    try { 
        foo(i < count - 1000 ? new Date() : "a");
        x = false;
    } catch (e) {
        x = true;
    }
}

if (!x)
    throw "bad result: "+ x;