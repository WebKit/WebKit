function foo() {
    // If the structure check hoister is overzealous, it will hoist the structure check of
    // S3 up to the assignment of o, where the structure is actually S1. Doing so will cause
    // this function to OSR exit every time.
    
    var o = {}; // This will create an object with structure S1.
    o.f = 42; // This will check for S1, and then transition to S2.
    o.g = 43; // This will check for S2, and then transition to S3.
    for (var i = 0; i < 5; ++i)
        o.g++; // This will have a structure check on structure S3, which the structure check hoister will want to hoist.
    return o;
}

var result = 0;
for (var i = 0; i < 100000; ++i)
    result += foo().g;

if (result != 4800000)
    throw ("Error: bad result: " + result);

