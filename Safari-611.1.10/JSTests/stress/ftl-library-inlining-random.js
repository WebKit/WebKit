function foo(x){
    return Math.random(x);
}

noInline(foo);

var x = 0;

for (var i = 0 ; i < 100000; i++){
    x = foo(i);
}
