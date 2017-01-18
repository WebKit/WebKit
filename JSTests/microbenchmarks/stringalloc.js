var global;
var array = ["a", "b"];
for (var i = 0; i < 10000000; ++i)
    global = array[i & 1] + "c";
