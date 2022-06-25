var array = [ "a", "b", "c", "d" ];

function foo(array, s) {
    for (var i = 0; i < array.length; ++i) {
        if (array[i] == s)
            return true;
    }
    return false;
}

noInline(foo);

var result = 0;
for (var i = 0; i < 100000; ++i)
    result += foo(array, "d");

if (result != 100000)
    throw "Bad result: " + result;
