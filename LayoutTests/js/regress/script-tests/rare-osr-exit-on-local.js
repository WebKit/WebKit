// This program attempts to relatively rare OSR exits due to mispredictions
// on the value of a local variable, where the basic block in which the local
// is assigned the offending value is different than the basic block in which
// the misspeculation occurs. The occurrence of the value that causes
// speculation failure is rare enough that the old JIT's value profiler is
// unlikely to catch it, but common enough that recompilation will be
// triggered.
//
// If the local was defined and used in the same basic block, then OSR exit
// would update the value profile associated with the assignment, and
// everything would be fine. But in this case OSR exit will see that the value
// comes from a GetLocal. If our mechanisms for updating the type predictions
// of local variables whose live ranges span basic blocks work, then it will
// only take one recompile for the optimizing compiler to converge to an
// optimal version of this code, where the variable is known to be one of two
// types and we optimize for both.
//
// TL;DR: This tests that OSR exit updates type predictions on local variables.

function foo(o,p) {
    var x;
    // Assign to the value on one of two basic blocks.
    if (p)
        x = o.f;
    else
        x = o.g;
    var y = !x;
    var z = o.h;
    var w = 0;
    for (var i = 0; i < 10000; ++i)
        w += z[i%10];
    return w + (y?1:0);
}

var array = [1,2,3,4,5,6,7,8,9,10];

var result = 0;

for (var i = 0; i < 300; ++i) {
    var v = i;
    if (i <= 100 || (i%4))
        v = {f:{h:v}, g:{h:v+1}, h:array};
    else
        v = {f:(i%3)==0, g:((i+1)%3)==0, h:array};
    result += foo(v,(i%2)==0);
}

if (result != 16500033)
    throw "Bad result: " + result;


