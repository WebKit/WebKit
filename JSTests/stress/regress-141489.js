// this test checks that register r9 is not a callee save on ios armv7.
function ident(a) { 
    return a; 
}

function foo(array,obj) { 
    var a = array[0]; 
    var b = array[1]; 
    var c = array[2]; 
    obj.a = array;
    obj.b = array;
    obj.c = array;
    obj.d = array;
    obj.e = array;
    obj.f = array;
    obj.h = array;
    return a(b(c(10)));
}
noInline(foo);

var arr = [ident,ident,ident];

for (var i = 0; i < 100; i++) {
    var obj = {};
    for (var j = 0; j < 200; j ++) {
        obj["j"+j] = i;
    }
    foo(arr, obj);
}

for (var i = 0; i < 100; i++) {
    var obj = {};
    foo(arr, obj);
}