function foo(x) {
    return Math.sqrt(x);
}

noInline(foo);

var j = 0;
for (var i = 0; i < 100000; ++i)
    j = foo(i);

if ( 316.226184874055 != j){
    throw "Error"
}
