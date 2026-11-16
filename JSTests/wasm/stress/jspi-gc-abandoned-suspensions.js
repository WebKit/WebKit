//@ requireOptions("--useJSPI=1")

import { instantiate } from "../wabt-wrapper.js"

// When a suspending promise is abandoned without being resolved, the associated objects
// should be garbage-collected even if there are reference cycles from conservative roots
// on the evacuated stack back to the suspending promise.

let wat = `
(module
  (import "env" "suspend" (func $suspend (param externref) (result i32)))
  (func $entry (export "entry") (param $obj externref) (result i32)
    local.get $obj
    call $suspend
  )
)`;

const count = 200;
let weakRefs = [];

{
    const instance = await instantiate(wat, {
        env: {
            suspend: new WebAssembly.Suspending(function (obj) {
                let promise = new Promise(() => {});
                obj.promise = promise;
                return promise;
            })
        }
    });
    const entry = WebAssembly.promising(instance.exports.entry);

    for (let i = 0; i < count; i++) {
        let obj = { index: i };
        weakRefs.push(new WeakRef(obj));
        entry(obj);
        // Don't store the returned promise — abandon it immediately.
    }
}
// instance, entry, obj, and all promises are now unreferenced.

// Use setTimeout to cross a job boundary (required by the WeakRef spec
// for targets to become clearable), then trigger a full GC.
setTimeout(() => {
    fullGC();

    let collected = 0;
    for (let weakRef of weakRefs) {
        if (weakRef.deref() === undefined)
            collected++;
    }

    if (collected == 0)
        throw new Error("Expected at least some abandoned JSPI objects to be collected, but none were");
}, 0);
