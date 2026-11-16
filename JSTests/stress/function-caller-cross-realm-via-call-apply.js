function assertCallerName(expectedName) {
    const { caller } = assertCallerName;
    if (typeof caller !== "function")
        throw new Error("caller should be a function!");
    if (caller.name !== expectedName)
        throw new Error(`caller.name should be "${expectedName}"! Got "${caller.name}" instead.`);
}

(function sameRealm() { assertCallerName("sameRealm"); })();  
(function sameRealmViaCall() { assertCallerName.call(null, "sameRealmViaCall"); })();  
(function sameRealmViaApply() { assertCallerName.apply(null, ["sameRealmViaApply"]); })();  

const other = $vm.createGlobalObject();
other.top = globalThis;
other.eval(`
    (function crossRealm() { top.assertCallerName("crossRealm"); })();
    (function crossRealmViaCall() { top.assertCallerName.call(null, "crossRealmViaCall"); })();
    (function crossRealmViaApply() { top.assertCallerName.apply(null, ["crossRealmViaApply"]); })();
`);
