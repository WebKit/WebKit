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
//    3. Loader.translate
//    4. Loader.instantiate

function setStateToMax(entry, newState)
{
    // https://whatwg.github.io/loader/#set-state-to-max

    "use strict";

    if (entry.state < newState)
        entry.state = newState;
}

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
    // 2. Translate
    //     Ready to translate (or now translating) the raw fetched resource to the ECMAScript source code.
    //     We can insert the hook that translates the resources e.g. transpilers.
    //     a. If the status is Translate and there is no entry.translate promise, the entry is ready to translate.
    //     b. If the status is Translate and there is the entry.translate promise, the entry is just translating
    //        the payload to the source code.
    //
    // 3. Instantiate (AnalyzeModule)
    //     Ready to instantiate (or now instantiating) the module record from the fetched (and translated)
    //     source code.
    //     Typically, we parse the module code, extract the dependencies and binding information.
    //     a. If the status is Instantiate and there is no entry.instantiate promise, the entry is ready to instantiate.
    //     b. If the status is Instantiate and there is the entry.translate promise, the entry is just instantiating
    //        the module record.
    //
    // 4. Satisfy
    //     Ready to request the dependent modules (or now requesting & resolving).
    //     Without this state, the current draft causes infinite recursion when there is circular dependency.
    //     a. If the status is Satisfy and there is no entry.satisfy promise, the entry is ready to resolve the dependencies.
    //     b. If the status is Satisfy and there is the entry.satisfy promise, the entry is just resolving
    //        the dependencies.
    //
    // 5. Link
    //     Ready to link the module with the other modules.
    //     Linking means that the module imports and exports the bindings from/to the other modules.
    //
    // 6. Ready
    //     The module is linked, so the module is ready to be executed.
    //
    // Each registry entry has the 4 promises; "fetch", "translate", "instantiate" and "satisfy".
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
        metadata: @undefined,
        fetch: @undefined,
        translate: @undefined,
        instantiate: @undefined,
        satisfy: @undefined,
        dependencies: [], // To keep the module order, we store the module keys in the array.
        dependenciesMap: @undefined,
        module: @undefined, // JSModuleRecord
        error: @undefined,
    };
}

function ensureRegistered(key)
{
    // https://whatwg.github.io/loader/#ensure-registered

    "use strict";

    var entry = this.registry.@get(key);
    if (entry)
        return entry;

    entry = this.newRegistryEntry(key);
    this.registry.@set(key, entry);

    return entry;
}

function forceFulfillPromise(promise, value)
{
    "use strict";

    if (promise.@promiseState === @promiseStatePending)
        @fulfillPromise(promise, value);
}

function fulfillFetch(entry, payload)
{
    // https://whatwg.github.io/loader/#fulfill-fetch

    "use strict";

    if (!entry.fetch)
        entry.fetch = @newPromiseCapability(@InternalPromise).@promise;
    this.forceFulfillPromise(entry.fetch, payload);
    this.setStateToMax(entry, @ModuleTranslate);
}

function fulfillTranslate(entry, source)
{
    // https://whatwg.github.io/loader/#fulfill-translate

    "use strict";

    if (!entry.translate)
        entry.translate = @newPromiseCapability(@InternalPromise).@promise;
    this.forceFulfillPromise(entry.translate, source);
    this.setStateToMax(entry, @ModuleInstantiate);
}

function fulfillInstantiate(entry, optionalInstance, source)
{
    // https://whatwg.github.io/loader/#fulfill-instantiate

    "use strict";

    if (!entry.instantiate)
        entry.instantiate = @newPromiseCapability(@InternalPromise).@promise;
    this.commitInstantiated(entry, optionalInstance, source);

    // FIXME: The draft fulfills the promise in the CommitInstantiated operation.
    // But it CommitInstantiated is also used in the requestInstantiate and
    // we should not "force fulfill" there.
    // So we separate "force fulfill" operation from the CommitInstantiated operation.
    // https://github.com/whatwg/loader/pull/67
    this.forceFulfillPromise(entry.instantiate, entry);
}

function commitInstantiated(entry, optionalInstance, source)
{
    // https://whatwg.github.io/loader/#commit-instantiated

    "use strict";

    var moduleRecord = this.instantiation(optionalInstance, source, entry);

    // FIXME: Described in the draft,
    //   4. Fulfill entry.[[Instantiate]] with instance.
    // But, instantiate promise should be fulfilled with the entry.
    // We remove this statement because instantiate promise will be
    // fulfilled without this "force fulfill" operation.
    // https://github.com/whatwg/loader/pull/67

    var dependencies = [];
    var dependenciesMap = moduleRecord.dependenciesMap;
    moduleRecord.registryEntry = entry;
    var requestedModules = this.requestedModules(moduleRecord);
    for (var i = 0, length = requestedModules.length; i < length; ++i) {
        var depKey = requestedModules[i];
        var pair = {
            key: depKey,
            value: @undefined
        };
        dependencies.@push(pair);
        dependenciesMap.@set(depKey, pair);
    }
    entry.dependencies = dependencies;
    entry.dependenciesMap = dependenciesMap;
    entry.module = moduleRecord;
    this.setStateToMax(entry, @ModuleSatisfy);
}

function instantiation(result, source, entry)
{
    // https://whatwg.github.io/loader/#instantiation
    // FIXME: Current implementation does not support optionalInstance.
    // https://bugs.webkit.org/show_bug.cgi?id=148171

    "use strict";

    return this.parseModule(entry.key, source);
}

// Loader.

function requestFetch(key, initiator)
{
    // https://whatwg.github.io/loader/#request-fetch

    "use strict";

    var entry = this.ensureRegistered(key);
    if (entry.state > @ModuleLink) {
        var deferred = @newPromiseCapability(@InternalPromise);
        deferred.@reject.@call(@undefined, new @TypeError("Requested module is already ready to be executed."));
        return deferred.@promise;
    }

    if (entry.fetch)
        return entry.fetch;

    // Hook point.
    // 2. Loader.fetch
    //     https://whatwg.github.io/loader/#browser-fetch
    //     Take the key and fetch the resource actually.
    //     For example, JavaScriptCore shell can provide the hook fetching the resource
    //     from the local file system.
    var fetchPromise = this.fetch(key, initiator).then((payload) => {
        this.setStateToMax(entry, @ModuleTranslate);
        return payload;
    });
    entry.fetch = fetchPromise;
    return fetchPromise;
}

function requestTranslate(key, initiator)
{
    // https://whatwg.github.io/loader/#request-translate

    "use strict";

    var entry = this.ensureRegistered(key);
    if (entry.state > @ModuleLink) {
        var deferred = @newPromiseCapability(@InternalPromise);
        deferred.@reject.@call(@undefined, new @TypeError("Requested module is already ready to be executed."));
        return deferred.@promise;
    }

    if (entry.translate)
        return entry.translate;

    var translatePromise = this.requestFetch(key, initiator).then((payload) => {
        // Hook point.
        // 3. Loader.translate
        //     https://whatwg.github.io/loader/#browser-translate
        //     Take the key and the fetched source code and translate it to the ES6 source code.
        //     Typically it is used by the transpilers.
        return this.translate(key, payload, initiator).then((source) => {
            this.setStateToMax(entry, @ModuleInstantiate);
            return source;
        });
    });
    entry.translate = translatePromise;
    return translatePromise;
}

function requestInstantiate(key, initiator)
{
    // https://whatwg.github.io/loader/#request-instantiate

    "use strict";

    var entry = this.ensureRegistered(key);
    if (entry.state > @ModuleLink) {
        var deferred = @newPromiseCapability(@InternalPromise);
        deferred.@reject.@call(@undefined, new @TypeError("Requested module is already ready to be executed."));
        return deferred.@promise;
    }

    if (entry.instantiate)
        return entry.instantiate;

    var instantiatePromise = this.requestTranslate(key, initiator).then((source) => {
        // Hook point.
        // 4. Loader.instantiate
        //     https://whatwg.github.io/loader/#browser-instantiate
        //     Take the key and the translated source code, and instantiate the module record
        //     by parsing the module source code.
        //     It has the chance to provide the optional module instance that is different from
        //     the ordinary one.
        return this.instantiate(key, source, initiator).then((optionalInstance) => {
            this.commitInstantiated(entry, optionalInstance, source);
            return entry;
        });
    });
    entry.instantiate = instantiatePromise;
    return instantiatePromise;
}

function requestSatisfy(key, initiator)
{
    // https://whatwg.github.io/loader/#satisfy-instance

    "use strict";

    var entry = this.ensureRegistered(key);
    if (entry.state > @ModuleLink) {
        var deferred = @newPromiseCapability(@InternalPromise);
        deferred.@reject.@call(@undefined, new @TypeError("Requested module is already ready to be executed."));
        return deferred.@promise;
    }

    if (entry.satisfy)
        return entry.satisfy;

    var satisfyPromise = this.requestInstantiate(key, initiator).then((entry) => {
        var depLoads = [];
        for (var i = 0, length = entry.dependencies.length; i < length; ++i) {
            let pair = entry.dependencies[i];

            // Hook point.
            // 1. Loader.resolve.
            //     https://whatwg.github.io/loader/#browser-resolve
            //     Take the name and resolve it to the unique identifier for the resource location.
            //     For example, take the "jquery" and return the URL for the resource.
            var promise = this.resolve(pair.key, key, initiator).then((depKey) => {
                var depEntry = this.ensureRegistered(depKey);

                // Recursive resolving. The dependencies of this entry is being resolved or already resolved.
                // Stop tracing the circular dependencies.
                // But to retrieve the instantiated module record correctly,
                // we need to wait for the instantiation for the dependent module.
                // For example, reaching here, the module is starting resolving the dependencies.
                // But the module may or may not reach the instantiation phase in the loader's pipeline.
                // If we wait for the Satisfy for this module, it construct the circular promise chain and
                // rejected by the Promises runtime. Since only we need is the instantiated module, instead of waiting
                // the Satisfy for this module, we just wait Instantiate for this.
                if (depEntry.satisfy) {
                    return depEntry.instantiate.then((entry) => {
                        pair.value = entry.module;
                        return entry;
                    });
                }

                return this.requestSatisfy(depKey, initiator).then((entry) => {
                    pair.value = entry.module;
                    return entry;
                });
            });
            depLoads.@push(promise);
        }

        return @InternalPromise.internalAll(depLoads).then((modules) => {
            this.setStateToMax(entry, @ModuleLink);
            return entry;
        });
    });

    entry.satisfy = satisfyPromise;
    return satisfyPromise;
}

function requestInstantiateAll(key, initiator)
{
    // https://whatwg.github.io/loader/#request-instantiate-all

    "use strict";

    return this.requestSatisfy(key, initiator);
}

function requestLink(key, initiator)
{
    // https://whatwg.github.io/loader/#request-link

    "use strict";

    var entry = this.ensureRegistered(key);
    if (entry.state > @ModuleLink) {
        var deferred = @newPromiseCapability(@InternalPromise);
        deferred.@resolve.@call(@undefined, entry);
        return deferred.@promise;
    }

    return this.requestInstantiateAll(key, initiator).then((entry) => {
        this.link(entry, initiator);
        return entry;
    });
}

function requestReady(key, initiator)
{
    // https://whatwg.github.io/loader/#request-ready

    "use strict";

    return this.requestLink(key, initiator).then((entry) => {
        this.moduleEvaluation(entry.module, initiator);
    });
}

// Linking semantics.

function link(entry, initiator)
{
    // https://whatwg.github.io/loader/#link

    "use strict";

    // FIXME: Current implementation does not support optionalInstance.
    // So Link's step 3 is skipped.
    // https://bugs.webkit.org/show_bug.cgi?id=148171

    if (entry.state === @ModuleReady)
        return;
    this.setStateToMax(entry, @ModuleReady);

    // Since we already have the "dependencies" field,
    // we can call moduleDeclarationInstantiation with the correct order
    // without constructing the dependency graph by calling dependencyGraph.
    var dependencies = entry.dependencies;
    for (var i = 0, length = dependencies.length; i < length; ++i) {
        var pair = dependencies[i];
        this.link(pair.value.registryEntry, initiator);
    }

    this.moduleDeclarationInstantiation(entry.module, initiator);
}

// Module semantics.

function moduleEvaluation(moduleRecord, initiator)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-moduleevaluation

    "use strict";

    if (moduleRecord.evaluated)
        return;
    moduleRecord.evaluated = true;

    var entry = moduleRecord.registryEntry;

    // The contents of the [[RequestedModules]] is cloned into entry.dependencies.
    var dependencies = entry.dependencies;
    for (var i = 0, length = dependencies.length; i < length; ++i) {
        var pair = dependencies[i];
        var requiredModuleRecord = pair.value;
        this.moduleEvaluation(requiredModuleRecord, initiator);
    }
    this.evaluate(entry.key, moduleRecord, initiator);
}

// APIs to control the module loader.

function provide(key, stage, value)
{
    "use strict";

    var entry = this.ensureRegistered(key);

    if (stage === @ModuleFetch) {
        if (entry.state > @ModuleFetch)
            throw new @TypeError("Requested module is already fetched.");
        this.fulfillFetch(entry, value);
        return;
    }

    if (stage === @ModuleTranslate) {
        if (entry.state > @ModuleTranslate)
            throw new @TypeError("Requested module is already translated.");
        this.fulfillFetch(entry, @undefined);
        this.fulfillTranslate(entry, value);
        return;
    }

    if (stage === @ModuleInstantiate) {
        if (entry.state > @ModuleInstantiate)
            throw new @TypeError("Requested module is already instantiated.");
        this.fulfillFetch(entry, @undefined);
        this.fulfillTranslate(entry, value);
        entry.translate.then((source) => {
            this.fulfillInstantiate(entry, value, source);
        });
        return;
    }

    throw new @TypeError("Requested module is already ready to be executed.");
}

function loadAndEvaluateModule(moduleName, referrer, initiator)
{
    "use strict";

    // Loader.resolve hook point.
    // resolve: moduleName => Promise(moduleKey)
    // Take the name and resolve it to the unique identifier for the resource location.
    // For example, take the "jquery" and return the URL for the resource.
    return this.resolve(moduleName, referrer, initiator).then((key) => {
        return this.requestReady(key, initiator);
    });
}

function loadModule(moduleName, referrer, initiator)
{
    "use strict";

    // Loader.resolve hook point.
    // resolve: moduleName => Promise(moduleKey)
    // Take the name and resolve it to the unique identifier for the resource location.
    // For example, take the "jquery" and return the URL for the resource.
    return this.resolve(moduleName, referrer, initiator).then((key) => {
        return this.requestInstantiateAll(key, initiator);
    }).then((entry) => {
        return entry.key;
    });
}

function linkAndEvaluateModule(key, initiator)
{
    "use strict";

    var entry = this.ensureRegistered(key);
    if (entry.state < @ModuleLink)
        throw new @TypeError("Requested module is not instantiated yet.");

    this.link(entry, initiator);
    return this.moduleEvaluation(entry.module, initiator);
}
