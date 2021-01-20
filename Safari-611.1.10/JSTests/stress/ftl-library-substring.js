function foo(i, x){
    return x.substring( 2 , 5);
}

noInline(foo);

var x = "";

for (var i = 0 ; i < 100000; i++){
    x = foo(i, "lkajsx");
}

if (x != "ajs")
    throw "Error: bad substring: "+ x;

