//@ requireOptions("--useClassFields=true")

class A {
    a = 0;
    b = 1;
    e = function () { return 0; };
    ["g"] = 0;
    h = eval("true");
}

for (var i = 0; i < 10000; i++)
    new A();

fullGC();

for (var i = 0; i < 10000; i++)
    new A();
