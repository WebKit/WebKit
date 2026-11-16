// The impl cache must never elide a spec-mandated user-visible side effect on a
// repeated construction: any shape that is not a plain string/undefined locale
// with undefined options is non-cacheable and must re-run its observable steps
// (Proxy traps, getters, indexed reads) on every call.

function twiceMustObserve(label, makeArgs)
{
    let calls = 0;
    const bump = () => { ++calls; };
    new Intl.DateTimeFormat(...makeArgs(bump));
    const afterFirst = calls;
    if (!afterFirst)
        throw new Error(`${label}: no observable side effect on first construction`);
    new Intl.DateTimeFormat(...makeArgs(bump));
    if (calls === afterFirst)
        throw new Error(`${label}: cache hit elided the side effect on second construction`);
}

// Proxy on the locales argument (a Proxy is not a plain string, so non-cacheable).
twiceMustObserve("proxy locales", (bump) =>
    [new Proxy(["en-US"], { get(t, p, r) { bump(); return Reflect.get(t, p, r); } })]);

// Proxy on the options argument.
twiceMustObserve("proxy options", (bump) =>
    ["en-US", new Proxy({}, { get(t, p, r) { bump(); return Reflect.get(t, p, r); } })]);

// Getter on a plain options object.
twiceMustObserve("getter options", (bump) => {
    const opts = {};
    Object.defineProperty(opts, "hour", { enumerable: true, get() { bump(); return "numeric"; } });
    return ["en-US", opts];
});

// Array locales with an indexed getter.
twiceMustObserve("array indexed getter", (bump) => {
    const arr = [];
    Object.defineProperty(arr, "0", { get() { bump(); return "en-US"; }, configurable: true });
    Object.defineProperty(arr, "length", { value: 1 });
    return [arr];
});

// null locales must throw on every call, never short-circuit to a cached success.
for (let i = 0; i < 2; ++i) {
    let threw = false;
    try { new Intl.DateTimeFormat(null); } catch { threw = true; }
    if (!threw)
        throw new Error(`null locales: expected throw on call ${i}`);
}

// Empty options resolves identically to absent options (a swallowed `options` would break this).
if (new Intl.DateTimeFormat("en-US", {}).resolvedOptions().locale
    !== new Intl.DateTimeFormat("en-US").resolvedOptions().locale)
    throw new Error("empty options did not resolve like absent options");
