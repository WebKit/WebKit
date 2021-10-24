"use strict";
const r1 = createGlobalObject();
const r2 = createGlobalObject();
const r3 = createGlobalObject();

(function() {
    for (const key of ["Array", "Date", "Error", "Function", "Int8Array", "Map", "Number", "Object", "RegExp", "WeakSet"]) {
        for (let i = 0; i < 1000; i++) {
            for (const newTarget of [
                r2[key].bind(),
                new r2.Function,
                new r2.Proxy(new r2.Function, {}),
            ]) {
                Object.defineProperty(newTarget, "prototype", { value: new r3.Object });
                const instance = Reflect.construct(r1[key], [], newTarget);
                if ($vm.globalObjectForObject(instance) !== r3)
                    throw new Error(`Structure of ${key} instance has incorrect global object!`);
            }
        }
    }
})();
