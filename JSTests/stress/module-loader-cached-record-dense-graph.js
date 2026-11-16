//@ runDefault

// Dense `export * from` graph where the same modules are requested from many
// referrers. Exercises the InnerModuleLoading path where a request resolves
// against an already-loaded module record (mapEntry->record() set), and the
// ModuleGraphLoadingError reaction is skipped. All namespaces must still resolve.

var abort = $vm.abort;

(async function () {
    const ns = await import("./resources/module-cached-record/dense-entry.js");
    if (ns.shared !== 42 || ns.a !== "a" || ns.b !== "b" || ns.c !== "c")
        throw new Error("bad namespace: " + JSON.stringify(Object.keys(ns)));

    // Re-import every node — every edge is now against a fully-loaded module.
    for (const file of ["shared", "a", "b", "c", "dense-entry"]) {
        const m = await import(`./resources/module-cached-record/${file}.js`);
        if (m.shared !== 42)
            throw new Error(`bad re-import of ${file}`);
    }
}()).catch((error) => {
    print(String(error));
    print(error.stack);
    abort();
});
