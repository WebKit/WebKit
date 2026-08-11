function foo(){
    o = Error();
    for (var s in o) {
        o[s];
        o = Error();
    }
}
noInline(foo);

for(var i = 0; i < 100; i++)
    foo();
