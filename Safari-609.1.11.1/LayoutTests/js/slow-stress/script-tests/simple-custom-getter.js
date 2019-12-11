var cycles = 20000
var numberObject = Number
function foo() {
    var result = 0;
    var innerCycles = cycles;
    var Number = numberObject;
    for (var i = 0; i < innerCycles; ++i)
        result += 0 | isNaN(Number.NaN);

    return result;
}
noInline(foo);
var result = 0;
for (var i = 0; i < 1500; i++)
    result += foo();
if (result != i * cycles)
    throw "Failed, result was " + (result) + " should be " + (i * cycles)
