var array = [ "a", "b", "c", "d" ];

function addFoo(x) { return x + "foo"; }

for (var i = 0; i < array.length; ++i)
    array[i] = addFoo(array[i]);

function foo(array, s) {
    for (var i = 0; i < array.length; ++i) {
        if (array[i] == s)
            return true;
    }
    return false;
}

var dFoo = addFoo("d");

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(array, dFoo);

if (result != 1000000)
    throw "Bad result: " + result;
