//@ requireOptions("--usePublicClassFields=true", "--usePrivateClassFields=true")

class A {
    a = 0;
    b = 1;
    #c = 0;
    #d = 1;
    e = function () { return 0; };
    #f = function() { return 1; };
    ["g"] = 0;
    h = eval("true");
    #i = eval("false");
}

for (var i = 0; i < 10000; i++)
    new A();

fullGC();

for (var i = 0; i < 10000; i++)
    new A();
