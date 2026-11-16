//@ runDefault

// Error propagation through a shared module whose record is already cached.
//
// m-broken-dep.js parses successfully (so its ModuleRegistryEntry has a record),
// but its dependency does-not-exist.js fails to fetch. After the first import
// fails, the second import requests m-broken-dep.js with mapEntry->record()
// already set — InnerModuleLoading skips the ModuleGraphLoadingError reaction
// on the cached loadPromise. The failure must still be observed via the
// recursion frontier.

var abort = $vm.abort;

async function shouldReject(promise) {
    try {
        await promise;
    } catch (e) {
        return e;
    }
    throw new Error("did not reject");
}

(async function () {
    // First import: m-broken-dep.js is fetched, parsed, and its record set;
    // the broken dependency rejects this load.
    const e1 = await shouldReject(import("./resources/module-cached-record/referrer-a.js"));

    // Second import: m-broken-dep.js's record is now cached. The error must
    // still propagate via the synchronously re-entered InnerModuleLoading.
    const e2 = await shouldReject(import("./resources/module-cached-record/referrer-b.js"));

    // Direct re-import of the shared module also rejects.
    const e3 = await shouldReject(import("./resources/module-cached-record/m-broken-dep.js"));

    for (const e of [e1, e2, e3]) {
        if (!String(e).includes("does-not-exist"))
            throw new Error("unexpected error: " + e);
    }
}()).catch((error) => {
    print(String(error));
    print(error.stack);
    abort();
});
