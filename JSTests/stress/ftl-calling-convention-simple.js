var o1 = {
    f: function() {
        return 100;    
    }
}

var o2 = {
    f: function() {
        return 200;    
    }
}

function foo(o) {
    return o.f() + 1;
}
noInline(foo);
noDFG(foo);
noInline(o1.f);
noDFG(o1.f);
noInline(o2.f);
noDFG(o2.f);

function main() {
    for (var i = 0; i < 2; i++)
        foo(i == 0 ? o1 : o2);
}

noInline(main)
for (let i = 0; i < 10000; ++i)
    main()