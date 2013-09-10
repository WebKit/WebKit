//@ runDefault

var array = [ "a", "b", "c", "d" ];

function foo(array, s) {
    for (var i = 0; i < array.length; ++i) {
        if (array[i] == s)
            return true;
    }
    return false;
}

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(array, "d");

if (result != 1000000)
    throw "Bad result: " + result;
