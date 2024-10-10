/*
 * Copyright (C) 2015-2024 Apple Inc. All rights reserved.
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

@alwaysInline
@linkTimeConstant
function ensureImportMapsLoaded(callback)
{
    "use strict";

    var status = @importMapStatus();
    return status === @undefined ? callback() : status.then(callback);
}

@alwaysInline
@visibility=PrivateRecursive
function getModuleMapEntry(key, moduleType)
{
    "use strict";

    if (moduleType === @ModuleTypeJavaScript)
        return this.moduleMap.@get(key);

    for (var i = 0, length = this.nonJSModuleArray.length; i < length; i++) {
        var entry = this.nonJSModuleArray[i];
        if (entry.key === key && entry.moduleType === moduleType)
            return entry;
    }
}

@visibility=PrivateRecursive
function ensureModuleMapEntry(key, moduleType)
{
    "use strict";

    var entry = this.getModuleMapEntry(key, moduleType);
    if (entry)
        return entry;

    // https://tc39.es/ecma262/#cyclic-module-record
    entry = {
        key,
        moduleType,
        status: @ModuleStatusNew,
        errorToRethrow: null,
        fetchAndParsePromise: null,
        moduleRecord: null,
        evaluationError: null,
        dfsIndex: -1,
        dfsAncestorIndex: -1,
        requestedModules: [],
        requestedModuleParameters: null,
        loadedModules: null,
        cycleRoot: null,
        hasTLA: false, // has top-level `await`
        asyncEvaluation: 0, // disguises as boolean
        asyncEvaluationWasEverTrue: false, // debug-only
        topLevelCapability: null,
        asyncParentModules: [],
        pendingAsyncDependencies: -1,
        __proto__: null,
    };

    if (moduleType === @ModuleTypeJavaScript)
        this.moduleMap.@set(key, entry);        
    else
        @arrayPush(this.nonJSModuleArray, entry);

    return entry;
}

// https://tc39.es/ecma262/#sec-HostLoadImportedModule
@visibility=PrivateRecursive
function hostFetchAndLoadImportedModule(key, parameters, fetcher, state)
{
    "use strict";

    var moduleType = parameters === @undefined ? @ModuleTypeJavaScript : this.getModuleType(parameters);
    var entry = this.ensureModuleMapEntry(key, moduleType);

    if (entry.fetchAndParsePromise === null) {
        entry.fetchAndParsePromise = this.fetch(key, parameters, fetcher).then(sourceCode => {
            return this.parseModule(key, sourceCode).then(moduleRecord => this.hostLoadImportedModule(entry, moduleRecord), error => {
                entry.errorToRethrow = error;
                if (state)
                    state.parseError = error;
                return entry;
            });
        });
    }

    return entry.fetchAndParsePromise.catch(error => {
        var clonedError = @makeTypeError(error.message);
        clonedError.@failureKind = error.@failureKind;
        throw clonedError;
    });
}

// https://tc39.es/ecma262/#sec-HostLoadImportedModule
@visibility=PrivateRecursive
function hostLoadImportedModule(entry, moduleRecord)
{
    "use strict";

    entry.moduleRecord = moduleRecord;
    entry.requestedModules = this.requestedModules(moduleRecord);
    entry.requestedModuleParameters = this.requestedModuleParameters(moduleRecord);
    entry.loadedModules = moduleRecord.dependenciesMap;

    @assert(entry.requestedModules.length === entry.requestedModuleParameters.length);
    @assert(entry.loadedModules.@size === 0);

    return entry;
}

// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-and-link-a-module-script
// https://tc39.es/ecma262/#sec-LoadRequestedModules
@visibility=PrivateRecursive
function fetchDescendantsOfAndLink(entry, fetcher)
{
    "use strict";

    var promiseCapability = @newPromiseCapability(@InternalPromise);
    var state = {
        isLoading: true,
        pendingModulesCount: 1,
        visited: new @Set,
        parseError: null,
        promiseCapability,
        fetcher,
    };

    if (entry.errorToRethrow) {
        promiseCapability.reject(entry.errorToRethrow);
        return promiseCapability;
    }

    this.innerModuleLoading(state, entry);

    return promiseCapability.promise.then(() => {
        if (state.parseError) {
            entry.errorToRethrow = state.parseError;
            throw state.parseError;
        }

        try {
            this.moduleLinking(entry, fetcher);
        } catch (error) {
            entry.errorToRethrow = error;
            throw error;
        }
    });
}

// https://tc39.es/ecma262/#sec-InnerModuleLoading
@visibility=PrivateRecursive
function innerModuleLoading(state, entry)
{
    "use strict";

    // This implements step 3 of ContinueModuleLoading
    const continueModuleLoadingAbrupt = error => {
        state.isLoading = false;
        state.promiseCapability.reject(error);
    };

    @assert(state.isLoading);

    if (entry.status === @ModuleStatusNew && !state.visited.@has(entry)) {
        state.visited.@add(entry);
        state.pendingModulesCount += entry.requestedModules.length;

        var fetcher = state.fetcher;
        for (var i = 0, length = entry.requestedModules.length; i < length; i++) {
            const specifier = entry.requestedModules[i];
            var loadedEntry = entry.loadedModules.@get(specifier);
            if (loadedEntry) {
                if (loadedEntry.errorToRethrow)
                    state.parseError = loadedEntry.errorToRethrow;

                this.innerModuleLoading(state, loadedEntry);
            } else {
                try {
                    var key = this.resolve(specifier, entry.key, fetcher);
                } catch (error) {
                    // This implements step 9.3 of https://html.spec.whatwg.org/multipage/webappapis.html#creating-a-javascript-module-script
                    entry.errorToRethrow = error;
                    throw error;
                }

                this.hostFetchAndLoadImportedModule(key, entry.requestedModuleParameters[i], fetcher, state).then(loadedEntry => {
                    // This implements step 1 of FinishLoadingImportedModule
                    @assert(!entry.loadedModules.@has(specifier) || entry.loadedModules.@get(specifier) === loadedEntry);
                    entry.loadedModules.@set(specifier, loadedEntry);

                    // This implements steps 1-2 of ContinueModuleLoading
                    if (state.isLoading)
                        this.innerModuleLoading(state, loadedEntry);
                }, continueModuleLoadingAbrupt);
            }
        }

        if (!state.isLoading)
            return;
    }

    @assert(state.pendingModulesCount >= 1);
    state.pendingModulesCount--;
    if (state.pendingModulesCount === 0) {
        state.isLoading = false;
        state.visited.@forEach(loadedEntry => {
            if (loadedEntry.status === @ModuleStatusNew)
                loadedEntry.status = @ModuleStatusUnlinked;
        });
        state.promiseCapability.resolve();
    }
}

// https://tc39.es/ecma262/#sec-moduledeclarationlinking
@visibility=PrivateRecursive
function moduleLinking(entry, fetcher)
{
    "use strict";

    @assert(entry.status === @ModuleStatusUnlinked || entry.status === @ModuleStatusLinked || entry.status === @ModuleStatusEvaluatingAsync || entry.status === @ModuleStatusEvaluated);

    var stack = [];

    try {
        this.innerModuleLinking(entry, stack, 0, fetcher);
    } catch (error) {
        for (var i = 0, length = stack.length; i < length; i++) {
            var stackEntry = stack[i];
            @assert(stackEntry.status === @ModuleStatusLinking);
            stackEntry.status = @ModuleStatusUnlinked;
        }

        throw error;
    }

    @assert(entry.status === @ModuleStatusLinked || entry.status === @ModuleStatusEvaluatingAsync || entry.status === @ModuleStatusEvaluated);
    @assert(stack.length === 0);
}

// https://tc39.es/ecma262/#sec-InnerModuleLinking
@visibility=PrivateRecursive
function innerModuleLinking(entry, stack, index, fetcher)
{
    "use strict";

    if (entry.status === @ModuleStatusLinking || entry.status === @ModuleStatusLinked || entry.status === @ModuleStatusEvaluatingAsync || entry.status === @ModuleStatusEvaluated)
        return index;

    @assert(entry.status === @ModuleStatusUnlinked);
    entry.status = @ModuleStatusLinking;
    entry.dfsIndex = index;
    entry.dfsAncestorIndex = index;
    index++;
    @arrayPush(stack, entry);

    for (var i = 0, length = entry.requestedModules.length; i < length; i++) {
        var specifier = entry.requestedModules[i];
        var loadedEntry = entry.loadedModules.@get(specifier); // This implements step 2 of GetImportedModule
        @assert(!!loadedEntry);
        index = this.innerModuleLinking(loadedEntry, stack, index, fetcher);
        @assert(loadedEntry.status === @ModuleStatusLinking || loadedEntry.status === @ModuleStatusLinked || loadedEntry.status === @ModuleStatusEvaluatingAsync || loadedEntry.status === @ModuleStatusEvaluated);
        @assert((loadedEntry.status === @ModuleStatusLinking) === @arrayIncludes(stack, loadedEntry));

        if (loadedEntry.status === @ModuleStatusLinking)
            entry.dfsAncestorIndex = @min(entry.dfsAncestorIndex, loadedEntry.dfsAncestorIndex);
    }

    if (!entry.moduleRecord && entry.errorToRethrow)
        throw entry.errorToRethrow;

    entry.hasTLA = this.linkModuleRecord(entry.moduleRecord, fetcher);

    @assert(@arrayCountOf(stack, entry) === 1);
    @assert(entry.dfsAncestorIndex <= entry.dfsIndex);

    if (entry.dfsAncestorIndex === entry.dfsIndex) {
        for (;;) {
            var requiredModule = @arrayPop(stack);
            requiredModule.status = @ModuleStatusLinked;
            if (requiredModule === entry)
                break;
        }
    }

    return index;
}

// https://tc39.es/ecma262/#sec-moduleevaluation
@visibility=PrivateRecursive
function moduleEvaluation(entry, fetcher)
{
    "use strict";

    if (entry.errorToRethrow) {
        var capability = @newPromiseCapability(@InternalPromise);
        capability.reject(entry.errorToRethrow);
        return capability.promise;
    }

    @assert(entry.status === @ModuleStatusLinked || entry.status === @ModuleStatusEvaluatingAsync || entry.status === @ModuleStatusEvaluated);

    // https://searchfox.org/mozilla-central/rev/1f5e1875cbfd5d4b1bfa27ca54832f62dd19589e/js/src/vm/Modules.cpp#1431
    if (entry.evaluationError !== null) {
        var capability = entry.topLevelCapability;
        if (capability === null) {
            capability = entry.topLevelCapability = @newPromiseCapability(@InternalPromise);
            capability.reject(entry.evaluationError.value);
        }

        return capability.promise;
    }

    if (entry.status === @ModuleStatusEvaluatingAsync || entry.status === @ModuleStatusEvaluated) {
        entry = entry.cycleRoot;
        @assert(!!entry);
    }

    if (entry.topLevelCapability !== null)
        return entry.topLevelCapability.promise;

    var stack = [];
    var capability = entry.topLevelCapability = @newPromiseCapability(@InternalPromise);

    try {
        this.innerModuleEvaluation(entry, stack, 0, fetcher);

        @assert(entry.status === @ModuleStatusEvaluatingAsync || entry.status === @ModuleStatusEvaluated);
        @assert(entry.evaluationError === null);

        if (!entry.asyncEvaluation) {
            @assert(entry.status === @ModuleStatusEvaluated);
            capability.resolve();
        }

        @assert(stack.length === 0);
    } catch (error) {
        for (var i = 0, length = stack.length; i < length; i++) {
            var stackEntry = stack[i];
            @assert(stackEntry.status === @ModuleStatusEvaluating);
            stackEntry.status = @ModuleStatusEvaluated;
            stackEntry.evaluationError = { value: error };
        }

        @assert(entry.status === @ModuleStatusEvaluated);
        @assert(entry.evaluationError.value === error);
        capability.reject(error);
    }

    return capability.promise;
}

// https://tc39.es/ecma262/#sec-innermoduleevaluation
@visibility=PrivateRecursive
function innerModuleEvaluation(entry, stack, index, fetcher)
{
    "use strict";

    if (entry.errorToRethrow)
        throw entry.errorToRethrow;

    if (entry.status === @ModuleStatusEvaluatingAsync || entry.status === @ModuleStatusEvaluated) {
        if (entry.evaluationError === null)
            return index;
        throw entry.evaluationError.value;
    }

    if (entry.status === @ModuleStatusEvaluating)
        return index;

    @assert(entry.status === @ModuleStatusLinked);

    entry.status = @ModuleStatusEvaluating;
    entry.dfsIndex = index;
    entry.dfsAncestorIndex = index;
    entry.pendingAsyncDependencies = 0;
    index++;
    @arrayPush(stack, entry);

    for (var i = 0, length = entry.requestedModules.length; i < length; i++) {
        var specifier = entry.requestedModules[i];
        var loadedEntry = entry.loadedModules.@get(specifier); // This implements step 2 of GetImportedModule
        @assert(!!loadedEntry);
        index = this.innerModuleEvaluation(loadedEntry, stack, index, fetcher);
        @assert(loadedEntry.status === @ModuleStatusEvaluating || loadedEntry.status === @ModuleStatusEvaluatingAsync || loadedEntry.status === @ModuleStatusEvaluated);
        @assert((loadedEntry.status === @ModuleStatusEvaluating) === @arrayIncludes(stack, loadedEntry));

        if (loadedEntry.status === @ModuleStatusEvaluating)
            entry.dfsAncestorIndex = @min(entry.dfsAncestorIndex, loadedEntry.dfsAncestorIndex);
        else {
            loadedEntry = loadedEntry.cycleRoot;
            @assert(loadedEntry.status === @ModuleStatusEvaluatingAsync || loadedEntry.status === @ModuleStatusEvaluated);
            if (loadedEntry.evaluationError !== null)
                throw loadedEntry.evaluationError.value;
        }

        if (loadedEntry.asyncEvaluation) {
            entry.pendingAsyncDependencies++;
            @arrayPush(loadedEntry.asyncParentModules, entry);
        }
    }

    if (entry.pendingAsyncDependencies > 0 || entry.hasTLA) {
        @assert(!entry.asyncEvaluation);
        @assert(!entry.asyncEvaluationWasEverTrue);

        entry.asyncEvaluation = this.nextAsyncEvaluationValue++;
        entry.asyncEvaluationWasEverTrue = true;
        if (entry.pendingAsyncDependencies === 0)
            this.executeAsyncModule(entry, fetcher);
    } else
        this.evaluate(entry.key, entry.moduleRecord, fetcher);

    @assert(@arrayCountOf(stack, entry) === 1);
    @assert(entry.dfsAncestorIndex <= entry.dfsIndex);

    if (entry.dfsAncestorIndex === entry.dfsIndex) {
        for (;;) {
            var requiredModule = @arrayPop(stack);
            requiredModule.status = requiredModule.asyncEvaluation ? @ModuleStatusEvaluatingAsync : @ModuleStatusEvaluated;
            requiredModule.cycleRoot = entry;
            if (requiredModule === entry)
                break;
        }
    }

    return index;
}

// https://tc39.es/ecma262/#sec-execute-async-module
@visibility=PrivateRecursive
async function executeAsyncModule(entry, fetcher)
{
    "use strict";

    @assert(entry.status === @ModuleStatusEvaluating || entry.status === @ModuleStatusEvaluatingAsync);
    @assert(entry.hasTLA);

    var key = entry.key;
    var moduleRecord = entry.moduleRecord;
    var sentValue;
    var resumeMode = @GeneratorResumeModeNormal;

    try {
        for (;;) {
            var result = this.evaluate(key, moduleRecord, fetcher, sentValue, resumeMode);
            if (@getAbstractModuleRecordInternalField(moduleRecord, @abstractModuleRecordFieldState) === @GeneratorStateExecuting)
                break;

            try {
                sentValue = await result;
                resumeMode = @GeneratorResumeModeNormal;
            } catch (error) {
                sentValue = error;
                resumeMode = @GeneratorResumeModeThrow;
            }
        }
    } catch (error) {
        @asyncModuleExecutionRejected(entry, error);
        return;
    }

    this.asyncModuleExecutionFulfilled(entry, fetcher);
}

// https://tc39.es/ecma262/#sec-gather-available-ancestors
@linkTimeConstant
function gatherAvailableAncestors(entry, execList)
{
    "use strict";

    for (var i = 0, length = entry.asyncParentModules; i < length; i++) {
        var parentEntry = entry.asyncParentModules[i];
        if (!@arrayIncludes(execList, parentEntry) && parentEntry.cycleRoot.evaluationError === null) {
            @assert(parentEntry.status === @ModuleStatusEvaluatingAsync);
            @assert(parentEntry.evaluationError === null);
            @assert(!!parentEntry.asyncEvaluation);
            @assert(parentEntry.pendingAsyncDependencies > 0);

            parentEntry.pendingAsyncDependencies--;
            if (parentEntry.pendingAsyncDependencies === 0) {
                @arrayPush(execList, parentEntry);
                if (!parentEntry.hasTLA)
                    @gatherAvailableAncestors(parentEntry, execList);
            }
        }
    }
}

// https://tc39.es/ecma262/#sec-async-module-execution-fulfilled
@visibility=PrivateRecursive
function asyncModuleExecutionFulfilled(entry, fetcher)
{
    "use strict";

    if (entry.status === @ModuleStatusEvaluated) {
        @assert(entry.evaluationError !== null);
        return;
    }

    @assert(entry.status === @ModuleStatusEvaluatingAsync);
    @assert(!!entry.asyncEvaluation);
    @assert(entry.evaluationError === null);

    // `asyncEvaluation` isn't reset per https://github.com/tc39/ecma262/pull/2824
    entry.status = @ModuleStatusEvaluated;

    if (entry.topLevelCapability !== null) {
        @assert(entry.cycleRoot === entry);
        entry.topLevelCapability.resolve();
    }

    var execList = [];
    @gatherAvailableAncestors(entry, execList);
    @arraySort.@call(execList, (a, b) => a.asyncEvaluation - b.asyncEvaluation);

    @assert((() => {
        for (var i = 0, length = execList.length; i < length; i++) {
            var entry = execList[i];

            @assert(!!entry.asyncEvaluation);
            @assert(entry.pendingAsyncDependencies === 0);
            @assert(entry.evaluationError === null);
        }

        return true;
    })());

    for (var i = 0, length = execList.length; i < length; i++) {
        var parentEntry = execList[i];
        if (parentEntry.status === @ModuleStatusEvaluated)
            @assert(parentEntry.evaluationError !== null);
        else if (parentEntry.hasTLA)
            this.executeAsyncModule(parentEntry, fetcher);
        else {
            try {
                this.evaluate(parentEntry.key, parentEntry.moduleRecord, fetcher);
                parentEntry.status = @ModuleStatusEvaluated;
                if (parentEntry.topLevelCapability !== null) {
                    @assert(parentEntry.cycleRoot === parentEntry);
                    parentEntry.topLevelCapability.resolve();
                }
            } catch (error) {
                @asyncModuleExecutionRejected(parentEntry, error);
            }
        }
    }
}

// https://tc39.es/ecma262/#sec-async-module-execution-rejected
@linkTimeConstant
function asyncModuleExecutionRejected(entry, error)
{
    "use strict";

    if (entry.status === @ModuleStatusEvaluated) {
        @assert(entry.evaluationError !== null);
        return;
    }

    @assert(entry.status === @ModuleStatusEvaluatingAsync);
    @assert(!!entry.asyncEvaluation);
    @assert(entry.evaluationError === null);

    entry.evaluationError = { value: error };
    entry.status = @ModuleStatusEvaluated;

    for (var i = 0, length = entry.asyncParentModules.length; i < length; i++) {
        var parentEntry = entry.asyncParentModules[i];
        @asyncModuleExecutionRejected(parentEntry, error);
    }

    if (entry.topLevelCapability !== null) {
        @assert(entry.cycleRoot === entry);
        entry.topLevelCapability.reject(error);
    }
}

// APIs to control the module loader.

@visibility=PrivateRecursive
function fetchModule(key, parameters, fetcher)
{
    "use strict";

    return @ensureImportMapsLoaded(() => {
        return this.hostFetchAndLoadImportedModule(key, parameters, fetcher).then(entry => {
            if (entry.errorToRethrow)
                throw entry.errorToRethrow;
            return this.fetchDescendantsOfAndLink(entry, fetcher);
        });
    });
}

@visibility=PrivateRecursive
function fetchModuleAndEvaluate(key, parameters, fetcher)
{
    "use strict";

    return this.fetchModule(key, parameters, fetcher).then(() => {
        var entry = this.getModuleMapEntry(key, this.getModuleType(parameters));
        @assert(!!entry);
        return this.moduleEvaluation(entry, fetcher);
    });
}

@visibility=PrivateRecursive
function evaluateModule(key, fetcher)
{
    "use strict";

    var entry = this.moduleMap.@get(key);
    @assert(!!entry);
    return this.moduleEvaluation(entry, fetcher);
}

@visibility=PrivateRecursive
function loadModule(key, sourceCode, fetcher)
{
    "use strict";

    var entry = this.ensureModuleMapEntry(key, @ModuleTypeJavaScript);
    if (entry.fetchAndParsePromise === null) {
        entry.fetchAndParsePromise = this.parseModule(key, sourceCode).then(moduleRecord => this.hostLoadImportedModule(entry, moduleRecord), error => {
            entry.errorToRethrow = error;
            return entry;
        });
    }

    return @ensureImportMapsLoaded(() => {
        return entry.fetchAndParsePromise.then(entry => {
            if (entry.errorToRethrow)
                throw entry.errorToRethrow;
            return this.fetchDescendantsOfAndLink(entry, fetcher);
        });
    });
}

@visibility=PrivateRecursive
function loadModuleAndEvaluate(key, sourceCode, fetcher)
{
    "use strict";

    return this.loadModule(key, sourceCode, fetcher).then(() => {
        var entry = this.moduleMap.@get(key);
        @assert(!!entry);
        return this.moduleEvaluation(entry, fetcher);
    });
}

@visibility=PrivateRecursive
function requestImportModule(specifier, referrer, parameters, fetcher)
{
    "use strict";

    return @ensureImportMapsLoaded(() => {
        var key = this.resolve(specifier, referrer, fetcher);

        return this.fetchModuleAndEvaluate(key, parameters, fetcher).then(() => {
            var entry = this.getModuleMapEntry(key, this.getModuleType(parameters));
            @assert(!!entry);
            return this.getModuleNamespaceObject(entry.moduleRecord);
        });
    });
}

@visibility=PrivateRecursive
function dependencyKeysIfEvaluated(key)
{
    "use strict";

    var entry = this.moduleMap.@get(key);
    if (!entry || entry.status !== @ModuleStatusEvaluated)
        return;

    var result = [];

    entry.loadedModules.@forEach(loadedEntry => {
        @arrayPush(result, loadedEntry.key);
    });

    return result;
}
