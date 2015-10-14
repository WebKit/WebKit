//@ runNoCJITNoAccessInlining

// Regression test for https://bugs.webkit.org/show_bug.cgi?id=148542
//
// In order to manifest, the bug being tested requires all these conditions to be true:
// 1. A put operation must not being optimized by the DFG into a PutByOffset.
//    It needs to be a PutById node instead so that it will use the inline cache.
//    This is satisfied by using the --useAccessInlining=false option above.
//
// 2. The PutById's execution must go through its transition stub.
//
// 3. In the transition stub, the object being put into must require a reallocation of its
//    storage butterfly. This causes the stub to generate code to save some registers.
//
// 4. The transition stub needs to call the slow path for flushing the heap write barrier
//    buffer.
//
// 5. The caller of the test must not be DFG compiled. This was not a strictly needed
//    condition of the bug, but allowing the caller to compile seems to interfere with
//    our method below of achieving condition 3.
//
// With the bug fixed, this test should not crash.

var val = { a: 5, b: 10 }

function test(obj, val, j, x, y, z) {
    obj.a = val.a; // PutById after GetById
    if (val.b)     // GetById to access val in a register again.
        val.b++;
}

noInline(test);

function runTest() {
    for (var j = 0; j < 50; j++) {
        var objs = [];

        let numberOfObjects = 200;
        for (var k = 0; k < numberOfObjects; k++) { 
            var obj = { };

            // Condition 3.
            // Fuzzing the amount of property storage used so that we can get the
            // repatch stub generator to resize the object out of line storage, and
            // create more register pressure to do that work. This in turn causes it to
            // need to preserve registers on the stack.
            var numInitialProps = j % 20;
            for (var i = 0; i < numInitialProps; i++)
                obj["i" + i] = i;

            objs[k] = obj;
        }

        // Condition 4.
        // Put all the objects in the GC's oldGen so that we can exercise the write
        // barrier when we exercise the PutById.
        gc();

        for (var k = 0; k < numberOfObjects; k++) {
            // Condition 2.
            // Eventually, the IC will converge on the slow path. Need to gc()
            // periodically to repatch anew.
            if (k % 97 == 1 && j % 5 == 1)
                gc();

            test(objs[k], val, j);
        }
    }
}

noDFG(runTest); // Condition 5.
runTest();
