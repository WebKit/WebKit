function foo(x){
    return { 0 : 1 , a : x }
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i)
    o = foo(i);

if (o.a != (testLoopCount - 1) || o[0] != 1)
    throw "Error"

