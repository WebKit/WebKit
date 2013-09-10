description(
"This tests that common subexpression elimination knows how to accurately model PutBuVal."
);

function doAccesses(a, b, i, j, y) {
    var x = a[i];
    b[j] = y;
    return a[i];
}

var array1 = [1, 2, 3, 4];
var array2 = [5, 6, 7, 8];

for (var i = 0; i < 1000; ++i) {
    // Simple test, pretty easy to pass.
    shouldBe("doAccesses(array1, array2, i % 4, (i + 1) % 4, i)", "" + ((i % 4) + 1));
    shouldBe("array2[" + ((i + 1) % 4) + "]", "" + i);
    
    // Undo.
    array2[((i + 1) % 4)] = (i % 4) + 5;
    
    // Now the evil test. This is constructed to minimize the likelihood that CSE will succeed through
    // cleverness alone.
    shouldBe("doAccesses(array1, array1, i % 4, 0, i)", "" + ((i % 4) == 0 ? i : (i % 4) + 1));
    
    // Undo.
    array1[0] = 1;
}

