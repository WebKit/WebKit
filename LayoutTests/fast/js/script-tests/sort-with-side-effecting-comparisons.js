description(
"Checks that sorting an array with a side-effecting comparison function doesn't trigger assertions."
);

var array = [];

for (var i = 0; i < 20000; ++i)
    array.push(i);

array.sort(function(a, b) {
    array.shift();
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
});

testPassed("It worked.");


