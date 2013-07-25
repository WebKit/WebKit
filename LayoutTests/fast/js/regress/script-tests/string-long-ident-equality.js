var array = [ "agdsagdwd", "bgbgbgbga", "cb3w53bq4", "dwerweeww" ];

function foo(array, s) {
    for (var i = 0; i < array.length; ++i) {
        if (array[i] == s)
            return true;
    }
    return false;
}

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(array, "cb3w53bq4");

if (result != 1000000)
    throw "Bad result: " + result;
