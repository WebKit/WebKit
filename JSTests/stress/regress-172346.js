function test(){
    var get = [];
    var p = new Proxy(Function(), { get:function(){ return Proxy; }});
    ({}) instanceof p;
}

var exception;
try {
    test();
} catch (e) {
    exception = e;
}

if (exception != "TypeError: calling Proxy constructor without new is invalid")
    throw "FAILED";

