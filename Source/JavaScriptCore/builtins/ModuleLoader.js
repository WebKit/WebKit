/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// https://whatwg.github.io/loader/#loader-object
// Module Loader has several hooks that can be customized by the platform.
// For example, the [[Fetch]] hook can be provided by the JavaScriptCore shell
// as fetching the payload from the local file system.
// Currently, there are 4 hooks.
//    1. Loader.resolve
//    2. Loader.fetch

@linkTimeConstant
function setStateToMax(entry, newState)
{
    // https://whatwg.github.io/loader/#set-state-to-max

    "use strict";

    if (entry.state < newState)
        entry.state = newState;
}

@linkTimeConstant
function newRegistryEntry(key)
{
    // https://whatwg.github.io/loader/#registry
    //
    // Each registry entry becomes one of the 5 states.
    // 1. Fetch
    //     Ready to fetch (or now fetching) the resource of this module.
    //     Typically, we fetch the source code over the network or from the file system.
    //     a. If the status is Fetch and there is no entry.fetch promise, the entry is ready to fetch.
    //     b. If the status is Fetch and there is the entry.fetch promise, the entry is just fetching the resource.
    //
    // 2. Instantiate (AnalyzeModule)
    //     Ready to instantiate (or now instantiating) the module record from the fetched
    //     source code.
    //     Typically, we parse the module code, extract the dependencies and binding information.
    //     a. If the status is Instantiate and there is no entry.instantiate promise, the entry is ready to instantiate.
    //     b. If the status is Instantiate and there is the entry.fetch promise, the entry is just instantiating
    //        the module record.
    //
    // 3. Satisfy
    //     Ready to request the dependent modules (or now requesting & resolving).
    //     Without this state, the current draft causes infinite recursion when there is circular dependency.
    //     a. If the status is Satisfy and there is no entry.satisfy promise, the entry is ready to resolve the dependencies.
    //     b. If the status is Satisfy and there is the entry.satisfy promise, the entry is just resolving
    //        the dependencies.
    //
    // 4. Link
    //     Ready to link the module with the other modules.
    //     Linking means that the module imports and exports the bindings from/to the other modules.
    //
    // 5. Ready
    //     The module is linked, so the module is ready to be executed.
    //
    // Each registry entry has the 4 promises; "fetch", "instantiate" and "satisfy".
    // They are assigned when starting the each phase. And they are fulfilled when the each phase is completed.
    //
    // In the current module draft, linking will be performed after the whole modules are instantiated and the dependencies are resolved.
    // And execution is also done after the all modules are linked.
    //
    // TODO: We need to exploit the way to execute the module while fetching non-related modules.
    // One solution; introducing the ready promise chain to execute the modules concurrently while keeping
    // the execution order.

    "use strict";

    return {
        key: key,
        state: @ModuleFetch,
        fetch: @undefined,
        instantiate: @undefined,
        satisfy: @undefined,
        dependencies: [], // To keep the module order, we store the module keys in the array.
        module: @undefined, // JSModuleRecord
        linkError: @undefined,
        linkSucceeded: true,
        evaluated: false,
        then: @undefined,
        isAsync: false,
    };
}

@visibility=PrivateRecursive
function ensureRegistered(key)
{
    // https://whatwg.github.io/loader/#ensure-registered

    "use strict";

    var entry = this.registry.@get(key);
    if (entry)
        return entry;

    entry = @newRegistryEntry(key);
    this.registry.@set(key, entry);

    return entry;
}

@linkTimeConstant
function forceFulfillPromise(promise, value)
{
    "use strict";

    @assert(@isPromise(promise));

    if ((@getPromiseInternalField(promise, @promiseFieldFlags) & @promiseStateMask) === @promiseStatePending)
        @fulfillPromise(promise, value);
}

@linkTimeConstant
function fulfillFetch(entry, source)
{
    // https://whatwg.github.io/loader/#fulfill-fetch

    "use strict";

    if (!entry.fetch)
        entry.fetch = @newPromiseCapability(@InternalPromise).@promise;
    @forceFulfillPromise(entry.fetch, source);
    @setStateToMax(entry, @ModuleInstantiate);
}

// Loader.

@visibility=PrivateRecursive
function requestFetch(entry, parameters, fetcher)
{
    // https://whatwg.github.io/loader/#request-fetch

    "use strict";

    if (entry.fetch) {
        var currentAttempt = entry.fetch;
        if (entry.state !== @ModuleFetch)
            return currentAttempt;

        return currentAttempt.catch((error) => {
            // Even if the existing fetching request failed, this attempt may succeed.
            // For example, previous attempt used invalid integrity="" value. But this
            // request could have the correct integrity="" value. In that case, we should
            // retry fetching for this request.
            // https://html.spec.whatwg.org/#fetch-a-single-module-script
            if (currentAttempt === entry.fetch)
                entry.fetch = @undefined;
            return this.requestFetch(entry, parameters, fetcher);
        });
    }

    // Hook point.
    // 2. Loader.fetch
    //     https://whatwg.github.io/loader/#browser-fetch
    //     Take the key and fetch the resource actually.
    //     For example, JavaScriptCore shell can provide the hook fetching the resource
    //     from the local file system.
    var fetchPromise = this.fetch(entry.key, parameters, fetcher).then((source) => {
        @setStateToMax(entry, @ModuleInstantiate);
        return source;
    });
    entry.fetch = fetchPromise;
    return fetchPromise;
}

@visibility=PrivateRecursive
function requestInstantiate(entry, parameters, fetcher)
{
    // https://whatwg.github.io/loader/#request-instantiate

    "use strict";

    // entry.instantiate is set if fetch succeeds.
    if (entry.instantiate)
        return entry.instantiate;

    var instantiatePromise = (async () => {
        var source = await this.requestFetch(entry, parameters, fetcher);
        // https://html.spec.whatwg.org/#fetch-a-single-module-script
        // Now fetching request succeeds. Then even if instantiation fails, we should cache it.
        // Instantiation won't be retried.
        if (entry.instantiate)
            return await entry.instantiate;
        entry.instantiate = instantiatePromise;

        var key = entry.key;
        var moduleRecord = await this.parseModule(key, source);
        var dependenciesMap = moduleRecord.dependenciesMap;
        var requestedModules = this.requestedModules(moduleRecord);
        var dependencies = @newArrayWithSize(requestedModules.length);
        for (var i = 0, length = requestedModules.length; i < length; ++i) {
            var depName = requestedModules[i];
            var depKey = this.resolve(depName, key, fetcher);
            var depEntry = this.ensureRegistered(depKey);
            @putByValDirect(dependencies, i, depEntry);
            dependenciesMap.@set(depName, depEntry);
        }
        entry.dependencies = dependencies;
        entry.module = moduleRecord;
        @setStateToMax(entry, @ModuleSatisfy);
        return entry;
    })();
    return instantiatePromise;
}

@visibility=PrivateRecursive
function requestSatisfy(entry, parameters, fetcher, visited)
{
    // https://html.spec.whatwg.org/#internal-module-script-graph-fetching-procedure

    "use strict";

    if (entry.satisfy)
        return entry.satisfy;

    visited.@add(entry);
    var satisfyPromise = this.requestInstantiate(entry, parameters, fetcher).then((entry) => {
        if (entry.satisfy)
            return entry.satisfy;

        var depLoads = this.requestedModuleParameters(entry.module);
        for (var i = 0, length = entry.dependencies.length; i < length; ++i) {
            var parameters = depLoads[i];
            var depEntry = entry.dependencies[i];
            var promise;

            // Recursive resolving. The dependencies of this entry is being resolved or already resolved.
            // Stop tracing the circular dependencies.
            // But to retrieve the instantiated module record correctly,
            // we need to wait for the instantiation for the dependent module.
            // For example, reaching here, the module is starting resolving the dependencies.
            // But the module may or may not reach the instantiation phase in the loader's pipeline.
            // If we wait for the Satisfy for this module, it construct the circular promise chain and
            // rejected by the Promises runtime. Since only we need is the instantiated module, instead of waiting
            // the Satisfy for this module, we just wait Instantiate for this.
            if (visited.@has(depEntry))
                promise = this.requestInstantiate(depEntry, parameters, fetcher);
            else {
                // Currently, module loader do not pass any information for non-top-level module fetching.
                promise = this.requestSatisfy(depEntry, parameters, fetcher, visited);
            }
            @putByValDirect(depLoads, i, promise);
        }

        return @InternalPromise.internalAll(depLoads).then((entries) => {
            if (entry.satisfy)
                return entry;
            @setStateToMax(entry, @ModuleLink);
            entry.satisfy = satisfyPromise;
            return entry;
        });
    });

    return satisfyPromise;
}

// Linking semantics.

@visibility=PrivateRecursive
function link(entry, fetcher)
{
    // https://html.spec.whatwg.org/#fetch-the-descendants-of-and-instantiate-a-module-script

    "use strict";

    if (!entry.linkSucceeded)
        throw entry.linkError;
    if (entry.state === @ModuleReady)
        return;
    @setStateToMax(entry, @ModuleReady);

    try {
        // Since we already have the "dependencies" field,
        // we can call moduleDeclarationInstantiation with the correct order
        // without constructing the dependency graph by calling dependencyGraph.
        var hasAsyncDependency = false;
        var dependencies = entry.dependencies;
        for (var i = 0, length = dependencies.length; i < length; ++i) {
            var dependency = dependencies[i];
            this.link(dependency, fetcher);
            hasAsyncDependency ||= dependency.isAsync;
        }

        entry.isAsync = this.moduleDeclarationInstantiation(entry.module, fetcher) || hasAsyncDependency;
    } catch (error) {
        entry.linkSucceeded = false;
        entry.linkError = error;
        throw error;
    }
}

// Module semantics.

@visibility=PrivateRecursive
function moduleEvaluation(entry, fetcher)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-moduleevaluation
    "use strict";

    if (entry.evaluated)
        return;
    entry.evaluated = true;

    // The contents of the [[RequestedModules]] is cloned into entry.dependencies.
    var dependencies = entry.dependencies;

    if (!entry.isAsync) {
        // Since linking sets isAsync for any strongly connected component with an async module we should only get here if all our dependencies are also sync.
        for (var i = 0, length = dependencies.length; i < length; ++i) {
            var dependency = dependencies[i];
            @assert(!dependency.isAsync);
            this.moduleEvaluation(dependency, fetcher);
        }

        this.evaluate(entry.key, entry.module, fetcher);
    } else
        return this.asyncModuleEvaluation(entry, fetcher, dependencies);
}

@visibility=PrivateRecursive
async function asyncModuleEvaluation(entry, fetcher, dependencies)
{
    "use strict";

    for (var i = 0, length = dependencies.length; i < length; ++i)
        await this.moduleEvaluation(dependencies[i], fetcher);

    var resumeMode = @GeneratorResumeModeNormal;
    while (true) {
        var awaitedValue = this.evaluate(entry.key, entry.module, fetcher, awaitedValue, resumeMode);
        if (@getAbstractModuleRecordInternalField(entry.module, @abstractModuleRecordFieldState) == @GeneratorStateExecuting)
            return;

        try {
            awaitedValue = await awaitedValue;
            resumeMode = @GeneratorResumeModeNormal;
        } catch (e) {
            awaitedValue = e;
            resumeMode = @GeneratorResumeModeThrow;
        }
    }
}

// APIs to control the module loader.

@visibility=PrivateRecursive
function provideFetch(key, value)
{
    "use strict";

    var entry = this.ensureRegistered(key);

    if (entry.state > @ModuleFetch)
        @throwTypeError("Requested module is already fetched.");
    @fulfillFetch(entry, value);
}

@visibility=PrivateRecursive
async function loadModule(key, parameters, fetcher)
{
    "use strict";

    var importMap = @importMapStatus();
    if (importMap)
        await importMap;
    var entry = await this.requestSatisfy(this.ensureRegistered(key), parameters, fetcher, new @Set);
    return entry.key;
}

@visibility=PrivateRecursive
function linkAndEvaluateModule(key, fetcher)
{
    "use strict";

    var entry = this.ensureRegistered(key);
    if (entry.state < @ModuleLink)
        @throwTypeError("Requested module is not instantiated yet.");

    this.link(entry, fetcher);
    return this.moduleEvaluation(entry, fetcher);
}

@visibility=PrivateRecursive
async function loadAndEvaluateModule(moduleName, parameters, fetcher)
{
    "use strict";

    var importMap = @importMapStatus();
    if (importMap)
        await importMap;
    var key = this.resolve(moduleName, @undefined, fetcher);
    key = await this.loadModule(key, parameters, fetcher);
    return await this.linkAndEvaluateModule(key, fetcher);
}

@visibility=PrivateRecursive
async function requestImportModule(moduleName, referrer, parameters, fetcher)
{
    "use strict";

    var importMap = @importMapStatus();
    if (importMap)
        await importMap;
    var key = this.resolve(moduleName, referrer, fetcher);
    var entry = await this.requestSatisfy(this.ensureRegistered(key), parameters, fetcher, new @Set);
    await this.linkAndEvaluateModule(entry.key, fetcher);
    return this.getModuleNamespaceObject(entry.module);
}

@visibility=PrivateRecursive
function dependencyKeysIfEvaluated(key)
{
    "use strict";

    var entry = this.registry.@get(key);
    if (!entry || !entry.evaluated)
        return null;

    var dependencies = entry.dependencies;
    var length = dependencies.length;
    var result = new @Array(length);
    for (var i = 0; i < length; ++i)
        result[i] = dependencies[i].key;

    return result;
}
