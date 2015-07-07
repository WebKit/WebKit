function foo(x){
    return { 0 : 1 , a : x }
}

noInline(foo);

for (var i = 0; i < 100000; ++i)
    o = foo(i);

if (o.a != 99999 || o[0] != 1)
    throw "Error"

