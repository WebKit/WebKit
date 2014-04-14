var count = 0;
function test() { return result ^ (count += 3); }
var result = 0;
for (var i = 0; i < 100000; i++)
     result = result ^ (i * test.bind(1,2)()) + 1;

if (result != 509992157)
    throw "Bad result: " + result;
