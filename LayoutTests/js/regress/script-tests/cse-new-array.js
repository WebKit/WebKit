function foo(p) {
    var array = [1.5, p];
    return Math.cos(Math.sqrt(Math.abs(Math.sin(array[0]) * 5 / 4.5))) % 3.5;
}
noInline(foo);

for (var i = 0; i < 100000; ++i)
    foo(0);
