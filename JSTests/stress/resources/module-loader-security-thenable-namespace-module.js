export var value = 42;

// This "then" export makes the namespace object a thenable per spec.
// When import() resolves with this namespace, the spec requires thenable
// unwrapping (ContinueDynamicImport step 6.d.ii).
export function then(resolve, reject) {
    // Validate that we receive proper resolve/reject functions.
    if (typeof resolve !== "function")
        throw new Error("resolve is not a function: " + typeof resolve);
    if (typeof reject !== "function")
        throw new Error("reject is not a function: " + typeof reject);

    // Validate that 'this' is the module namespace, not an internal object.
    if (this.value !== 42)
        throw new Error("'this' is not the namespace: value=" + this.value);

    // Check that resolve/reject are not internal objects via describe().
    var resolveDesc = describe(resolve);
    var rejectDesc = describe(reject);
    var internalTypes = ["JSSourceCode", "ModuleRegistryEntry", "ModuleLoadingContext",
                         "ModuleGraphLoadingState", "ModuleLoaderPayload"];
    for (var i = 0; i < internalTypes.length; i++) {
        if (resolveDesc.indexOf(internalTypes[i]) !== -1)
            throw new Error("resolve is internal object: " + resolveDesc);
        if (rejectDesc.indexOf(internalTypes[i]) !== -1)
            throw new Error("reject is internal object: " + rejectDesc);
    }
    if (/ModuleRecord(?!.*NamespaceObject)/.test(resolveDesc))
        throw new Error("resolve is ModuleRecord: " + resolveDesc);
    if (/ModuleRecord(?!.*NamespaceObject)/.test(rejectDesc))
        throw new Error("reject is ModuleRecord: " + rejectDesc);

    // Resolve with a non-thenable value to avoid infinite thenable unwrapping.
    // (Calling resolve(this) would loop because this namespace IS a thenable.)
    resolve({ value: this.value, isNamespaceResult: true });
}
