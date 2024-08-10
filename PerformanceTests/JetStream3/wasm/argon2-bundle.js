(function () {
    function r(e, n, t) {
        function o(i, f) {
            if (!n[i]) {
                if (!e[i]) {
                    var c = "function" == typeof require && require;
                    if (!f && c) return c(i, !0);
                    if (u) return u(i, !0);
                    var a = new Error("Cannot find module '" + i + "'");
                    throw a.code = "MODULE_NOT_FOUND", a
                }
                var p = n[i] = {
                    exports: {}
                };
                e[i][0].call(p.exports, function (r) {
                    var n = e[i][1][r];
                    return o(n || r)
                }, p, p.exports, r, e, n, t)
            }
            return n[i].exports
        }
        for (var u = "function" == typeof require && require, i = 0; i < t.length; i++) o(t[i]);
        return o
    }
    return r
})()({
    1: [function (require, module, exports) {
        // ************************ CHANGE START ************************
        globalThis.argon2 = require('argon2-browser');
        // ************************ CHANGE END ************************
    }, {
        "argon2-browser": 3
    }],
    2: [function (require, module, exports) {
        (function (process, __dirname) {
            (function () {
                var Argon2Module = typeof self !== "undefined" && typeof self.Argon2Module !== "undefined" ? self.Argon2Module : {};
                var jsModule = Argon2Module;
                var moduleOverrides = {};
                var key;
                for (key in Argon2Module) {
                    if (Argon2Module.hasOwnProperty(key)) {
                        moduleOverrides[key] = Argon2Module[key]
                    }
                }
                var arguments_ = [];
                var thisProgram = "./this.program";
                var quit_ = function (status, toThrow) {
                    throw toThrow
                };
                var ENVIRONMENT_IS_WEB = false;
                var ENVIRONMENT_IS_WORKER = false;
                var ENVIRONMENT_IS_NODE = false;
                var ENVIRONMENT_IS_SHELL = false;
                ENVIRONMENT_IS_WEB = typeof window === "object";
                ENVIRONMENT_IS_WORKER = typeof importScripts === "function";
                ENVIRONMENT_IS_NODE = typeof process === "object" && typeof process.versions === "object" && typeof process.versions.node === "string";
                ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;
                var scriptDirectory = "";

                function locateFile(path) {
                    if (Argon2Module["locateFile"]) {
                        return Argon2Module["locateFile"](path, scriptDirectory)
                    }
                    return scriptDirectory + path
                }
                var read_, readAsync, readBinary, setWindowTitle;
                var nodeFS;
                var nodePath;
                if (ENVIRONMENT_IS_NODE) {
                    if (ENVIRONMENT_IS_WORKER) {
                        scriptDirectory = require("path").dirname(scriptDirectory) + "/"
                    } else {
                        scriptDirectory = __dirname + "/"
                    }
                    read_ = function shell_read(filename, binary) {
                        if (!nodeFS) nodeFS = require("fs");
                        if (!nodePath) nodePath = require("path");
                        filename = nodePath["normalize"](filename);
                        return nodeFS["readFileSync"](filename, binary ? null : "utf8")
                    };
                    readBinary = function readBinary(filename) {
                        var ret = read_(filename, true);
                        if (!ret.buffer) {
                            ret = new Uint8Array(ret)
                        }
                        assert(ret.buffer);
                        return ret
                    };
                    if (process["argv"].length > 1) {
                        thisProgram = process["argv"][1].replace(/\\/g, "/")
                    }
                    arguments_ = process["argv"].slice(2);
                    if (typeof module !== "undefined") {
                        module["exports"] = Argon2Module
                    }
                    process["on"]("uncaughtException", function (ex) {
                        if (!(ex instanceof ExitStatus)) {
                            throw ex
                        }
                    });
                    process["on"]("unhandledRejection", abort);
                    quit_ = function (status) {
                        process["exit"](status)
                    };
                    Argon2Module["inspect"] = function () {
                        return "[Emscripten Argon2Module object]"
                    }
                } else if (ENVIRONMENT_IS_SHELL) {
                    if (typeof read != "undefined") {
                        read_ = function shell_read(f) {
                            return read(f)
                        }
                    }
                    readBinary = function readBinary(f) {
                        var data;
                        if (typeof readbuffer === "function") {
                            return new Uint8Array(readbuffer(f))
                        }
                        data = read(f, "binary");
                        assert(typeof data === "object");
                        return data
                    };
                    if (typeof scriptArgs != "undefined") {
                        arguments_ = scriptArgs
                    } else if (typeof arguments != "undefined") {
                        arguments_ = arguments
                    }
                    if (typeof quit === "function") {
                        quit_ = function (status) {
                            quit(status)
                        }
                    }
                    // ************************ CHANGE START ************************
                    // if (typeof print !== "undefined") {
                    //     if (typeof console === "undefined") console = {};
                    //     console.log = print;
                    //     console.warn = console.error = typeof printErr !== "undefined" ? printErr : print
                    // }
                    // ************************ CHANGE END ************************
                } else if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
                    if (ENVIRONMENT_IS_WORKER) {
                        scriptDirectory = self.location.href
                    } else if (typeof document !== "undefined" && document.currentScript) {
                        scriptDirectory = document.currentScript.src
                    }
                    if (scriptDirectory.indexOf("blob:") !== 0) {
                        scriptDirectory = scriptDirectory.substr(0, scriptDirectory.lastIndexOf("/") + 1)
                    } else {
                        scriptDirectory = ""
                    } {
                        read_ = function (url) {
                            var xhr = new XMLHttpRequest;
                            xhr.open("GET", url, false);
                            xhr.send(null);
                            return xhr.responseText
                        };
                        if (ENVIRONMENT_IS_WORKER) {
                            readBinary = function (url) {
                                var xhr = new XMLHttpRequest;
                                xhr.open("GET", url, false);
                                xhr.responseType = "arraybuffer";
                                xhr.send(null);
                                return new Uint8Array(xhr.response)
                            }
                        }
                        readAsync = function (url, onload, onerror) {
                            var xhr = new XMLHttpRequest;
                            xhr.open("GET", url, true);
                            xhr.responseType = "arraybuffer";
                            xhr.onload = function () {
                                if (xhr.status == 200 || xhr.status == 0 && xhr.response) {
                                    onload(xhr.response);
                                    return
                                }
                                onerror()
                            };
                            xhr.onerror = onerror;
                            xhr.send(null)
                        }
                    }
                    setWindowTitle = function (title) {
                        document.title = title
                    }
                } else { }
                var out = Argon2Module["print"] || console.log.bind(console);
                var err = Argon2Module["printErr"] || console.warn.bind(console);
                for (key in moduleOverrides) {
                    if (moduleOverrides.hasOwnProperty(key)) {
                        Argon2Module[key] = moduleOverrides[key]
                    }
                }
                moduleOverrides = null;
                if (Argon2Module["arguments"]) arguments_ = Argon2Module["arguments"];
                if (Argon2Module["thisProgram"]) thisProgram = Argon2Module["thisProgram"];
                if (Argon2Module["quit"]) quit_ = Argon2Module["quit"];
                var wasmBinary;
                if (Argon2Module["wasmBinary"]) wasmBinary = Argon2Module["wasmBinary"];
                var noExitRuntime = Argon2Module["noExitRuntime"] || true;
                if (typeof WebAssembly !== "object") {
                    abort("no native wasm support detected")
                }
                var wasmMemory;
                var ABORT = false;
                var EXITSTATUS;

                function assert(condition, text) {
                    if (!condition) {
                        abort("Assertion failed: " + text)
                    }
                }
                var ALLOC_NORMAL = 0;
                var ALLOC_STACK = 1;

                function allocate(slab, allocator) {
                    var ret;
                    if (allocator == ALLOC_STACK) {
                        ret = stackAlloc(slab.length)
                    } else {
                        ret = _malloc(slab.length)
                    }
                    if (slab.subarray || slab.slice) {
                        HEAPU8.set(slab, ret)
                    } else {
                        HEAPU8.set(new Uint8Array(slab), ret)
                    }
                    return ret
                }
                var UTF8Decoder = typeof TextDecoder !== "undefined" ? new TextDecoder("utf8") : undefined;

                function UTF8ArrayToString(heap, idx, maxBytesToRead) {
                    var endIdx = idx + maxBytesToRead;
                    var endPtr = idx;
                    while (heap[endPtr] && !(endPtr >= endIdx)) ++endPtr;
                    if (endPtr - idx > 16 && heap.subarray && UTF8Decoder) {
                        return UTF8Decoder.decode(heap.subarray(idx, endPtr))
                    } else {
                        var str = "";
                        while (idx < endPtr) {
                            var u0 = heap[idx++];
                            if (!(u0 & 128)) {
                                str += String.fromCharCode(u0);
                                continue
                            }
                            var u1 = heap[idx++] & 63;
                            if ((u0 & 224) == 192) {
                                str += String.fromCharCode((u0 & 31) << 6 | u1);
                                continue
                            }
                            var u2 = heap[idx++] & 63;
                            if ((u0 & 240) == 224) {
                                u0 = (u0 & 15) << 12 | u1 << 6 | u2
                            } else {
                                u0 = (u0 & 7) << 18 | u1 << 12 | u2 << 6 | heap[idx++] & 63
                            }
                            if (u0 < 65536) {
                                str += String.fromCharCode(u0)
                            } else {
                                var ch = u0 - 65536;
                                str += String.fromCharCode(55296 | ch >> 10, 56320 | ch & 1023)
                            }
                        }
                    }
                    return str
                }

                function UTF8ToString(ptr, maxBytesToRead) {
                    return ptr ? UTF8ArrayToString(HEAPU8, ptr, maxBytesToRead) : ""
                }

                function alignUp(x, multiple) {
                    if (x % multiple > 0) {
                        x += multiple - x % multiple
                    }
                    return x
                }
                var buffer, HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;

                function updateGlobalBufferAndViews(buf) {
                    buffer = buf;
                    Argon2Module["HEAP8"] = HEAP8 = new Int8Array(buf);
                    Argon2Module["HEAP16"] = HEAP16 = new Int16Array(buf);
                    Argon2Module["HEAP32"] = HEAP32 = new Int32Array(buf);
                    Argon2Module["HEAPU8"] = HEAPU8 = new Uint8Array(buf);
                    Argon2Module["HEAPU16"] = HEAPU16 = new Uint16Array(buf);
                    Argon2Module["HEAPU32"] = HEAPU32 = new Uint32Array(buf);
                    Argon2Module["HEAPF32"] = HEAPF32 = new Float32Array(buf);
                    Argon2Module["HEAPF64"] = HEAPF64 = new Float64Array(buf)
                }
                var INITIAL_MEMORY = Argon2Module["INITIAL_MEMORY"] || 16777216;
                var wasmTable;
                var __ATPRERUN__ = [];
                var __ATINIT__ = [];
                var __ATPOSTRUN__ = [];
                var runtimeInitialized = false;

                function preRun() {
                    if (Argon2Module["preRun"]) {
                        if (typeof Argon2Module["preRun"] == "function") Argon2Module["preRun"] = [Argon2Module["preRun"]];
                        while (Argon2Module["preRun"].length) {
                            addOnPreRun(Argon2Module["preRun"].shift())
                        }
                    }
                    callRuntimeCallbacks(__ATPRERUN__)
                }

                function initRuntime() {
                    runtimeInitialized = true;
                    callRuntimeCallbacks(__ATINIT__)
                }

                function postRun() {
                    if (Argon2Module["postRun"]) {
                        if (typeof Argon2Module["postRun"] == "function") Argon2Module["postRun"] = [Argon2Module["postRun"]];
                        while (Argon2Module["postRun"].length) {
                            addOnPostRun(Argon2Module["postRun"].shift())
                        }
                    }
                    callRuntimeCallbacks(__ATPOSTRUN__)
                }

                function addOnPreRun(cb) {
                    __ATPRERUN__.unshift(cb)
                }

                function addOnInit(cb) {
                    __ATINIT__.unshift(cb)
                }

                function addOnPostRun(cb) {
                    __ATPOSTRUN__.unshift(cb)
                }
                var runDependencies = 0;
                var runDependencyWatcher = null;
                var dependenciesFulfilled = null;

                function addRunDependency(id) {
                    runDependencies++;
                    if (Argon2Module["monitorRunDependencies"]) {
                        Argon2Module["monitorRunDependencies"](runDependencies)
                    }
                }

                function removeRunDependency(id) {
                    runDependencies--;
                    if (Argon2Module["monitorRunDependencies"]) {
                        Argon2Module["monitorRunDependencies"](runDependencies)
                    }
                    if (runDependencies == 0) {
                        if (runDependencyWatcher !== null) {
                            clearInterval(runDependencyWatcher);
                            runDependencyWatcher = null
                        }
                        if (dependenciesFulfilled) {
                            var callback = dependenciesFulfilled;
                            dependenciesFulfilled = null;
                            callback()
                        }
                    }
                }
                Argon2Module["preloadedImages"] = {};
                Argon2Module["preloadedAudios"] = {};

                function abort(what) {
                    if (Argon2Module["onAbort"]) {
                        Argon2Module["onAbort"](what)
                    }
                    what += "";
                    err(what);
                    ABORT = true;
                    EXITSTATUS = 1;
                    what = "abort(" + what + "). Build with -s ASSERTIONS=1 for more info.";
                    var e = new WebAssembly.RuntimeError(what);
                    throw e
                }
                var dataURIPrefix = "data:application/octet-stream;base64,";

                function isDataURI(filename) {
                    return filename.startsWith(dataURIPrefix)
                }

                function isFileURI(filename) {
                    return filename.startsWith("file://")
                }
                var wasmBinaryFile = "argon2.wasm";
                if (!isDataURI(wasmBinaryFile)) {
                    wasmBinaryFile = locateFile(wasmBinaryFile)
                }

                function getBinary(file) {
                    try {
                        if (file == wasmBinaryFile && wasmBinary) {
                            return new Uint8Array(wasmBinary)
                        }
                        if (readBinary) {
                            return readBinary(file)
                        } else {
                            throw "both async and sync fetching of the wasm failed"
                        }
                    } catch (err) {
                        abort(err)
                    }
                }

                function getBinaryPromise() {
                    if (!wasmBinary && (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER)) {
                        if (typeof fetch === "function" && !isFileURI(wasmBinaryFile)) {
                            return fetch(wasmBinaryFile, {
                                credentials: "same-origin"
                            }).then(function (response) {
                                if (!response["ok"]) {
                                    throw "failed to load wasm binary file at '" + wasmBinaryFile + "'"
                                }
                                return response["arrayBuffer"]()
                            }).catch(function () {
                                return getBinary(wasmBinaryFile)
                            })
                        } else {
                            if (readAsync) {
                                return new Promise(function (resolve, reject) {
                                    readAsync(wasmBinaryFile, function (response) {
                                        resolve(new Uint8Array(response))
                                    }, reject)
                                })
                            }
                        }
                    }
                    return Promise.resolve().then(function () {
                        return getBinary(wasmBinaryFile)
                    })
                }

                function createWasm() {
                    var info = {
                        "a": asmLibraryArg
                    };

                    function receiveInstance(instance, module) {
                        var exports = instance.exports;
                        Argon2Module["asm"] = exports;
                        wasmMemory = Argon2Module["asm"]["c"];
                        updateGlobalBufferAndViews(wasmMemory.buffer);
                        wasmTable = Argon2Module["asm"]["k"];
                        addOnInit(Argon2Module["asm"]["d"]);
                        removeRunDependency("wasm-instantiate")
                    }
                    addRunDependency("wasm-instantiate");

                    function receiveInstantiationResult(result) {
                        // ************************ CHANGE START ************************
                        if (result)
                        // ************************ CHANGE END ************************
                            receiveInstance(result["instance"])
                    }

                    function instantiateArrayBuffer(receiver) {
                        return getBinaryPromise().then(function (_) {
                            // ************************ CHANGE START ************************
                            let binary;
                            if (globalThis.argon2WasmSimdBlob || Module.argon2WasmSimdBlob) {
                                binary = globalThis.argon2WasmSimdBlob || Module.argon2WasmSimdBlob;
                                // console.warn("argon2 instantiating wasm simd binary");
                            }
                            if (globalThis.argon2WasmBlob || Module.argon2WasmBlob) {
                                binary = globalThis.argon2WasmBlob || Module.argon2WasmBlob;
                                // console.warn("argon2 instantiating wasm binary");
                            }
                            return WebAssembly.instantiate(binary, info);
                            // ************************ CHANGE END ************************
                        }).then(receiver, function (reason) {
                            err("failed to asynchronously prepare wasm: " + reason);
                            abort(reason)
                        })
                    }

                    function instantiateAsync() {
                        if (!wasmBinary && typeof WebAssembly.instantiateStreaming === "function" && !isDataURI(wasmBinaryFile) && !isFileURI(wasmBinaryFile) && typeof fetch === "function") {
                            return fetch(wasmBinaryFile, {
                                credentials: "same-origin"
                            }).then(function (response) {
                                var result = WebAssembly.instantiateStreaming(response, info);
                                return result.then(receiveInstantiationResult, function (reason) {
                                    err("wasm streaming compile failed: " + reason);
                                    err("falling back to ArrayBuffer instantiation");
                                    return instantiateArrayBuffer(receiveInstantiationResult)
                                })
                            })
                        } else {
                            return instantiateArrayBuffer(receiveInstantiationResult)
                        }
                    }
                    if (Argon2Module["instantiateWasm"]) {
                        try {
                            var exports = Argon2Module["instantiateWasm"](info, receiveInstance);
                            return exports
                        } catch (e) {
                            err("Argon2Module.instantiateWasm callback failed with error: " + e);
                            return false
                        }
                    }
                    instantiateAsync();
                    return {}
                }

                function callRuntimeCallbacks(callbacks) {
                    while (callbacks.length > 0) {
                        var callback = callbacks.shift();
                        if (typeof callback == "function") {
                            callback(Argon2Module);
                            continue
                        }
                        var func = callback.func;
                        if (typeof func === "number") {
                            if (callback.arg === undefined) {
                                wasmTable.get(func)()
                            } else {
                                wasmTable.get(func)(callback.arg)
                            }
                        } else {
                            func(callback.arg === undefined ? null : callback.arg)
                        }
                    }
                }

                function _emscripten_memcpy_big(dest, src, num) {
                    HEAPU8.copyWithin(dest, src, src + num)
                }

                function emscripten_realloc_buffer(size) {
                    try {
                        wasmMemory.grow(size - buffer.byteLength + 65535 >>> 16);
                        updateGlobalBufferAndViews(wasmMemory.buffer);
                        return 1
                    } catch (e) { }
                }

                function _emscripten_resize_heap(requestedSize) {
                    var oldSize = HEAPU8.length;
                    requestedSize = requestedSize >>> 0;
                    var maxHeapSize = 2147418112;
                    if (requestedSize > maxHeapSize) {
                        return false
                    }
                    for (var cutDown = 1; cutDown <= 4; cutDown *= 2) {
                        var overGrownHeapSize = oldSize * (1 + .2 / cutDown);
                        overGrownHeapSize = Math.min(overGrownHeapSize, requestedSize + 100663296);
                        var newSize = Math.min(maxHeapSize, alignUp(Math.max(requestedSize, overGrownHeapSize), 65536));
                        var replacement = emscripten_realloc_buffer(newSize);
                        if (replacement) {
                            return true
                        }
                    }
                    return false
                }
                var asmLibraryArg = {
                    "a": _emscripten_memcpy_big,
                    "b": _emscripten_resize_heap
                };
                var asm = createWasm();
                var ___wasm_call_ctors = Argon2Module["___wasm_call_ctors"] = function () {
                    return (___wasm_call_ctors = Argon2Module["___wasm_call_ctors"] = Argon2Module["asm"]["d"]).apply(null, arguments)
                };
                var _argon2_hash = Argon2Module["_argon2_hash"] = function () {
                    return (_argon2_hash = Argon2Module["_argon2_hash"] = Argon2Module["asm"]["e"]).apply(null, arguments)
                };
                var _malloc = Argon2Module["_malloc"] = function () {
                    return (_malloc = Argon2Module["_malloc"] = Argon2Module["asm"]["f"]).apply(null, arguments)
                };
                var _free = Argon2Module["_free"] = function () {
                    return (_free = Argon2Module["_free"] = Argon2Module["asm"]["g"]).apply(null, arguments)
                };
                var _argon2_verify = Argon2Module["_argon2_verify"] = function () {
                    return (_argon2_verify = Argon2Module["_argon2_verify"] = Argon2Module["asm"]["h"]).apply(null, arguments)
                };
                var _argon2_error_message = Argon2Module["_argon2_error_message"] = function () {
                    return (_argon2_error_message = Argon2Module["_argon2_error_message"] = Argon2Module["asm"]["i"]).apply(null, arguments)
                };
                var _argon2_encodedlen = Argon2Module["_argon2_encodedlen"] = function () {
                    return (_argon2_encodedlen = Argon2Module["_argon2_encodedlen"] = Argon2Module["asm"]["j"]).apply(null, arguments)
                };
                var _argon2_hash_ext = Argon2Module["_argon2_hash_ext"] = function () {
                    return (_argon2_hash_ext = Argon2Module["_argon2_hash_ext"] = Argon2Module["asm"]["l"]).apply(null, arguments)
                };
                var _argon2_verify_ext = Argon2Module["_argon2_verify_ext"] = function () {
                    return (_argon2_verify_ext = Argon2Module["_argon2_verify_ext"] = Argon2Module["asm"]["m"]).apply(null, arguments)
                };
                var stackAlloc = Argon2Module["stackAlloc"] = function () {
                    return (stackAlloc = Argon2Module["stackAlloc"] = Argon2Module["asm"]["n"]).apply(null, arguments)
                };
                Argon2Module["allocate"] = allocate;
                Argon2Module["UTF8ToString"] = UTF8ToString;
                Argon2Module["ALLOC_NORMAL"] = ALLOC_NORMAL;
                var calledRun;

                function ExitStatus(status) {
                    this.name = "ExitStatus";
                    this.message = "Program terminated with exit(" + status + ")";
                    this.status = status
                }
                dependenciesFulfilled = function runCaller() {
                    if (!calledRun) run();
                    if (!calledRun) dependenciesFulfilled = runCaller
                };

                function run(args) {
                    args = args || arguments_;
                    if (runDependencies > 0) {
                        return
                    }
                    preRun();
                    if (runDependencies > 0) {
                        return
                    }

                    function doRun() {
                        if (calledRun) return;
                        calledRun = true;
                        Argon2Module["calledRun"] = true;
                        if (ABORT) return;
                        initRuntime();
                        if (Argon2Module["onRuntimeInitialized"]) Argon2Module["onRuntimeInitialized"]();
                        postRun()
                    }
                    if (Argon2Module["setStatus"]) {
                        Argon2Module["setStatus"]("Running...");
                        setTimeout(function () {
                            setTimeout(function () {
                                Argon2Module["setStatus"]("")
                            }, 1);
                            doRun()
                        }, 1)
                    } else {
                        doRun()
                    }
                }
                Argon2Module["run"] = run;
                if (Argon2Module["preInit"]) {
                    if (typeof Argon2Module["preInit"] == "function") Argon2Module["preInit"] = [Argon2Module["preInit"]];
                    while (Argon2Module["preInit"].length > 0) {
                        Argon2Module["preInit"].pop()()
                    }
                }
                run();
                if (typeof module !== "undefined") module.exports = Argon2Module;
                Argon2Module.unloadRuntime = function () {
                    if (typeof self !== "undefined") {
                        delete self.Argon2Module
                    }
                    Argon2Module = jsModule = wasmMemory = wasmTable = asm = buffer = HEAP8 = HEAPU8 = HEAP16 = HEAPU16 = HEAP32 = HEAPU32 = HEAPF32 = HEAPF64 = undefined;
                    if (typeof module !== "undefined") {
                        delete module.exports
                    }
                };

            }).call(this)
        }).call(this, require('_process'), "/node_modules/argon2-browser/dist")
    }, {
        "_process": 9,
        "fs": 4,
        "path": 8
    }],
    3: [function (require, module, exports) {
        (function (Buffer) {
            (function () {
                "use strict";

                (function (root, factory) {
                    if (typeof define === 'function' && define.amd) {
                        define([], factory);
                    } else if (typeof module === 'object' && module.exports) {
                        module.exports = factory();
                    } else {
                        root.argon2 = factory();
                    }
                })(typeof self !== 'undefined' ? self : void 0, function () {
                    const global = typeof self !== 'undefined' ? self : this;

                    /**
                     * @enum
                     */
                    const ArgonType = {
                        Argon2d: 0,
                        Argon2i: 1,
                        Argon2id: 2
                    };

                    function loadModule(mem) {
                        if (loadModule._promise) {
                            return loadModule._promise;
                        }
                        if (loadModule._module) {
                            // console.log("loadModule cached");
                            return Promise.resolve(loadModule._module);
                        }
                        let promise;
                        if (global.process && global.process.versions && global.process.versions.node) {
                            promise = loadWasmModule().then(Argon2Module => new Promise(resolve => {
                                Argon2Module.postRun = () => resolve(Argon2Module);
                            }));
                        } else {
                            promise = loadWasmBinary().then(wasmBinary => {
                                const wasmMemory = mem ? createWasmMemory(mem) : undefined;
                                return initWasm(wasmBinary, wasmMemory);
                            });
                        }
                        loadModule._promise = promise;
                        return promise.then(Argon2Module => {
                            loadModule._module = Argon2Module;
                            delete loadModule._promise;
                            return Argon2Module;
                        });
                    }

                    function initWasm(wasmBinary, wasmMemory) {
                        return new Promise(resolve => {
                            global.Argon2Module = {
                                wasmBinary,
                                wasmMemory,
                                postRun() {
                                    resolve(Argon2Module);
                                }
                            };
                            return loadWasmModule();
                        });
                    }

                    function loadWasmModule() {
                        if (global.loadArgon2WasmModule) {
                            return global.loadArgon2WasmModule();
                        }
                        if (typeof require === 'function') {
                            return Promise.resolve(require('../dist/argon2.js'));
                        }
                        return import('../dist/argon2.js');
                    }

                    function loadWasmBinary() {
                        if (global.loadArgon2WasmBinary) {
                            return global.loadArgon2WasmBinary();
                        }

                        // ************************ CHANGE START ************************
                        // if (typeof require === 'function') {
                        //     return Promise.resolve(require('../dist/argon2.wasm')).then(
                        //         (wasmModule) => {
                        //             return decodeWasmBinary(wasmModule);
                        //         }
                        //     );
                        // }
                        //   const wasmPath = global.argon2WasmPath || 'node_modules/argon2-browser/dist/argon2.wasm';
                        //   return fetch(wasmPath).then(response => response.arrayBuffer()).then(ab => new Uint8Array(ab));
                        return Promise.resolve(new Uint8Array(0));
                        // ************************ CHANGE END ************************
                    }

                    // ************************ CHANGE START ************************
                    // function decodeWasmBinary(base64) {
                    //     const text = atob(base64);
                    //     const binary = new Uint8Array(new ArrayBuffer(text.length));
                    //     for (let i = 0; i < text.length; i++) {
                    //         binary[i] = text.charCodeAt(i);
                    //     }
                    //     return binary;
                    // }
                    // ************************ CHANGE END ************************

                    function createWasmMemory(mem) {
                        const KB = 1024;
                        const MB = 1024 * KB;
                        const GB = 1024 * MB;
                        const WASM_PAGE_SIZE = 64 * KB;
                        const totalMemory = (2 * GB - 64 * KB) / WASM_PAGE_SIZE;
                        const initialMemory = Math.min(Math.max(Math.ceil(mem * KB / WASM_PAGE_SIZE), 256) + 256, totalMemory);
                        return new WebAssembly.Memory({
                            initial: initialMemory,
                            maximum: totalMemory
                        });
                    }

                    function allocateArray(Argon2Module, arr) {
                        return Argon2Module.allocate(arr, 'i8', Argon2Module.ALLOC_NORMAL);
                    }

                    function allocateArrayStr(Argon2Module, arr) {
                        const nullTerminatedArray = new Uint8Array([...arr, 0]);
                        return allocateArray(Argon2Module, nullTerminatedArray);
                    }

                    function encodeUtf8(str) {
                        if (typeof str !== 'string') {
                            return str;
                        }
                        if (typeof TextEncoder === 'function') {
                            return new TextEncoder().encode(str);
                        } else if (typeof Buffer === 'function') {
                            return Buffer.from(str);
                        } else {
                            throw new Error("Don't know how to encode UTF8");
                        }
                    }

                    /**
                     * Argon2 hash
                     * @param {string|Uint8Array} params.pass - password string
                     * @param {string|Uint8Array} params.salt - salt string
                     * @param {number} [params.time=1] - the number of iterations
                     * @param {number} [params.mem=1024] - used memory, in KiB
                     * @param {number} [params.hashLen=24] - desired hash length
                     * @param {number} [params.parallelism=1] - desired parallelism
                     * @param {number} [params.type=argon2.ArgonType.Argon2d] - hash type:
                     *      argon2.ArgonType.Argon2d
                     *      argon2.ArgonType.Argon2i
                     *      argon2.ArgonType.Argon2id
                     *
                     * @return Promise
                     *
                     * @example
                     *  argon2.hash({ pass: 'password', salt: 'somesalt' })
                     *      .then(h => console.log(h.hash, h.hashHex, h.encoded))
                     *      .catch(e => console.error(e.message, e.code))
                     */
                    function argon2Hash(params) {
                        const mCost = params.mem || 1024;
                        return loadModule(mCost).then(Argon2Module => {
                            const tCost = params.time || 1;
                            const parallelism = params.parallelism || 1;
                            const pwdEncoded = encodeUtf8(params.pass);
                            const pwd = allocateArrayStr(Argon2Module, pwdEncoded);
                            const pwdlen = pwdEncoded.length;
                            const saltEncoded = encodeUtf8(params.salt);
                            const salt = allocateArrayStr(Argon2Module, saltEncoded);
                            const saltlen = saltEncoded.length;
                            const argon2Type = params.type || ArgonType.Argon2d;
                            const hash = Argon2Module.allocate(new Array(params.hashLen || 24), 'i8', Argon2Module.ALLOC_NORMAL);
                            const secret = params.secret ? allocateArray(Argon2Module, params.secret) : 0;
                            const secretlen = params.secret ? params.secret.byteLength : 0;
                            const ad = params.ad ? allocateArray(Argon2Module, params.ad) : 0;
                            const adlen = params.ad ? params.ad.byteLength : 0;
                            const hashlen = params.hashLen || 24;
                            const encodedlen = Argon2Module._argon2_encodedlen(tCost, mCost, parallelism, saltlen, hashlen, argon2Type);
                            const encoded = Argon2Module.allocate(new Array(encodedlen + 1), 'i8', Argon2Module.ALLOC_NORMAL);
                            const version = 0x13;
                            let err;
                            let res;
                            try {
                                res = Argon2Module._argon2_hash_ext(tCost, mCost, parallelism, pwd, pwdlen, salt, saltlen, hash, hashlen, encoded, encodedlen, argon2Type, secret, secretlen, ad, adlen, version);
                            } catch (e) {
                                err = e;
                            }
                            let result;
                            if (res === 0 && !err) {
                                let hashStr = '';
                                const hashArr = new Uint8Array(hashlen);
                                for (let i = 0; i < hashlen; i++) {
                                    const byte = Argon2Module.HEAP8[hash + i];
                                    hashArr[i] = byte;
                                    hashStr += ('0' + (0xff & byte).toString(16)).slice(-2);
                                }
                                const encodedStr = Argon2Module.UTF8ToString(encoded);
                                result = {
                                    hash: hashArr,
                                    hashHex: hashStr,
                                    encoded: encodedStr
                                };
                            } else {
                                try {
                                    if (!err) {
                                        err = Argon2Module.UTF8ToString(Argon2Module._argon2_error_message(res));
                                    }
                                } catch (e) { }
                                result = {
                                    message: err,
                                    code: res
                                };
                            }
                            try {
                                Argon2Module._free(pwd);
                                Argon2Module._free(salt);
                                Argon2Module._free(hash);
                                Argon2Module._free(encoded);
                                if (ad) {
                                    Argon2Module._free(ad);
                                }
                                if (secret) {
                                    Argon2Module._free(secret);
                                }
                            } catch (e) { }
                            if (err) {
                                throw result;
                            } else {
                                return result;
                            }
                        });
                    }

                    /**
                     * Argon2 verify function
                     * @param {string} params.pass - password string
                     * @param {string|Uint8Array} params.encoded - encoded hash
                     * @param {number} [params.type=argon2.ArgonType.Argon2d] - hash type:
                     *      argon2.ArgonType.Argon2d
                     *      argon2.ArgonType.Argon2i
                     *      argon2.ArgonType.Argon2id
                     *
                     * @returns Promise
                     *
                     * @example
                     *  argon2.verify({ pass: 'password', encoded: 'encoded-hash' })
                     *      .then(() => console.log('OK'))
                     *      .catch(e => console.error(e.message, e.code))
                     */
                    function argon2Verify(params) {
                        return loadModule().then(Argon2Module => {
                            const pwdEncoded = encodeUtf8(params.pass);
                            const pwd = allocateArrayStr(Argon2Module, pwdEncoded);
                            const pwdlen = pwdEncoded.length;
                            const secret = params.secret ? allocateArray(Argon2Module, params.secret) : 0;
                            const secretlen = params.secret ? params.secret.byteLength : 0;
                            const ad = params.ad ? allocateArray(Argon2Module, params.ad) : 0;
                            const adlen = params.ad ? params.ad.byteLength : 0;
                            const encEncoded = encodeUtf8(params.encoded);
                            const enc = allocateArrayStr(Argon2Module, encEncoded);
                            let argon2Type = params.type;
                            if (argon2Type === undefined) {
                                let typeStr = params.encoded.split('$')[1];
                                if (typeStr) {
                                    typeStr = typeStr.replace('a', 'A');
                                    argon2Type = ArgonType[typeStr] || ArgonType.Argon2d;
                                }
                            }
                            let err;
                            let res;
                            try {
                                res = Argon2Module._argon2_verify_ext(enc, pwd, pwdlen, secret, secretlen, ad, adlen, argon2Type);
                            } catch (e) {
                                err = e;
                            }
                            let result;
                            if (res || err) {
                                try {
                                    if (!err) {
                                        err = Argon2Module.UTF8ToString(Argon2Module._argon2_error_message(res));
                                    }
                                } catch (e) { }
                                result = {
                                    message: err,
                                    code: res
                                };
                            }
                            try {
                                Argon2Module._free(pwd);
                                Argon2Module._free(enc);
                            } catch (e) { }
                            if (err) {
                                throw result;
                            } else {
                                return result;
                            }
                        });
                    }

                    function unloadRuntime() {
                        if (loadModule._module) {
                            loadModule._module.unloadRuntime();
                            delete loadModule._promise;
                            delete loadModule._module;
                        }
                    }
                    return {
                        ArgonType,
                        hash: argon2Hash,
                        verify: argon2Verify,
                        unloadRuntime,
                        // ************************ CHANGE START ************************
                        loadModule,
                        // ************************ CHANGE END ************************
                    };
                });

            }).call(this)
        }).call(this, require("buffer").Buffer)
    }, {
        "../dist/argon2.js": 2,
        "buffer": 6
    }],
    4: [function (require, module, exports) {

    }, {}],
    5: [function (require, module, exports) {
        'use strict'

        exports.byteLength = byteLength
        exports.toByteArray = toByteArray
        exports.fromByteArray = fromByteArray

        var lookup = []
        var revLookup = []
        var Arr = typeof Uint8Array !== 'undefined' ? Uint8Array : Array

        var code = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
        for (var i = 0, len = code.length; i < len; ++i) {
            lookup[i] = code[i]
            revLookup[code.charCodeAt(i)] = i
        }

        // Support decoding URL-safe base64 strings, as Node.js does.
        // See: https://en.wikipedia.org/wiki/Base64#URL_applications
        revLookup['-'.charCodeAt(0)] = 62
        revLookup['_'.charCodeAt(0)] = 63

        function getLens(b64) {
            var len = b64.length

            if (len % 4 > 0) {
                throw new Error('Invalid string. Length must be a multiple of 4')
            }

            // Trim off extra bytes after placeholder bytes are found
            // See: https://github.com/beatgammit/base64-js/issues/42
            var validLen = b64.indexOf('=')
            if (validLen === -1) validLen = len

            var placeHoldersLen = validLen === len ?
                0 :
                4 - (validLen % 4)

            return [validLen, placeHoldersLen]
        }

        // base64 is 4/3 + up to two characters of the original data
        function byteLength(b64) {
            var lens = getLens(b64)
            var validLen = lens[0]
            var placeHoldersLen = lens[1]
            return ((validLen + placeHoldersLen) * 3 / 4) - placeHoldersLen
        }

        function _byteLength(b64, validLen, placeHoldersLen) {
            return ((validLen + placeHoldersLen) * 3 / 4) - placeHoldersLen
        }

        function toByteArray(b64) {
            var tmp
            var lens = getLens(b64)
            var validLen = lens[0]
            var placeHoldersLen = lens[1]

            var arr = new Arr(_byteLength(b64, validLen, placeHoldersLen))

            var curByte = 0

            // if there are placeholders, only get up to the last complete 4 chars
            var len = placeHoldersLen > 0 ?
                validLen - 4 :
                validLen

            var i
            for (i = 0; i < len; i += 4) {
                tmp =
                    (revLookup[b64.charCodeAt(i)] << 18) |
                    (revLookup[b64.charCodeAt(i + 1)] << 12) |
                    (revLookup[b64.charCodeAt(i + 2)] << 6) |
                    revLookup[b64.charCodeAt(i + 3)]
                arr[curByte++] = (tmp >> 16) & 0xFF
                arr[curByte++] = (tmp >> 8) & 0xFF
                arr[curByte++] = tmp & 0xFF
            }

            if (placeHoldersLen === 2) {
                tmp =
                    (revLookup[b64.charCodeAt(i)] << 2) |
                    (revLookup[b64.charCodeAt(i + 1)] >> 4)
                arr[curByte++] = tmp & 0xFF
            }

            if (placeHoldersLen === 1) {
                tmp =
                    (revLookup[b64.charCodeAt(i)] << 10) |
                    (revLookup[b64.charCodeAt(i + 1)] << 4) |
                    (revLookup[b64.charCodeAt(i + 2)] >> 2)
                arr[curByte++] = (tmp >> 8) & 0xFF
                arr[curByte++] = tmp & 0xFF
            }

            return arr
        }

        function tripletToBase64(num) {
            return lookup[num >> 18 & 0x3F] +
                lookup[num >> 12 & 0x3F] +
                lookup[num >> 6 & 0x3F] +
                lookup[num & 0x3F]
        }

        function encodeChunk(uint8, start, end) {
            var tmp
            var output = []
            for (var i = start; i < end; i += 3) {
                tmp =
                    ((uint8[i] << 16) & 0xFF0000) +
                    ((uint8[i + 1] << 8) & 0xFF00) +
                    (uint8[i + 2] & 0xFF)
                output.push(tripletToBase64(tmp))
            }
            return output.join('')
        }

        function fromByteArray(uint8) {
            var tmp
            var len = uint8.length
            var extraBytes = len % 3 // if we have 1 byte left, pad 2 bytes
            var parts = []
            var maxChunkLength = 16383 // must be multiple of 3

            // go through the array every three bytes, we'll deal with trailing stuff later
            for (var i = 0, len2 = len - extraBytes; i < len2; i += maxChunkLength) {
                parts.push(encodeChunk(uint8, i, (i + maxChunkLength) > len2 ? len2 : (i + maxChunkLength)))
            }

            // pad the end with zeros, but make sure to not forget the extra bytes
            if (extraBytes === 1) {
                tmp = uint8[len - 1]
                parts.push(
                    lookup[tmp >> 2] +
                    lookup[(tmp << 4) & 0x3F] +
                    '=='
                )
            } else if (extraBytes === 2) {
                tmp = (uint8[len - 2] << 8) + uint8[len - 1]
                parts.push(
                    lookup[tmp >> 10] +
                    lookup[(tmp >> 4) & 0x3F] +
                    lookup[(tmp << 2) & 0x3F] +
                    '='
                )
            }

            return parts.join('')
        }

    }, {}],
    6: [function (require, module, exports) {
        (function (Buffer) {
            (function () {
                /*!
                 * The buffer module from node.js, for the browser.
                 *
                 * @author   Feross Aboukhadijeh <https://feross.org>
                 * @license  MIT
                 */
                /* eslint-disable no-proto */

                'use strict'

                var base64 = require('base64-js')
                var ieee754 = require('ieee754')

                exports.Buffer = Buffer
                exports.SlowBuffer = SlowBuffer
                exports.INSPECT_MAX_BYTES = 50

                var K_MAX_LENGTH = 0x7fffffff
                exports.kMaxLength = K_MAX_LENGTH

                /**
                 * If `Buffer.TYPED_ARRAY_SUPPORT`:
                 *   === true    Use Uint8Array implementation (fastest)
                 *   === false   Print warning and recommend using `buffer` v4.x which has an Object
                 *               implementation (most compatible, even IE6)
                 *
                 * Browsers that support typed arrays are IE 10+, Firefox 4+, Chrome 7+, Safari 5.1+,
                 * Opera 11.6+, iOS 4.2+.
                 *
                 * We report that the browser does not support typed arrays if the are not subclassable
                 * using __proto__. Firefox 4-29 lacks support for adding new properties to `Uint8Array`
                 * (See: https://bugzilla.mozilla.org/show_bug.cgi?id=695438). IE 10 lacks support
                 * for __proto__ and has a buggy typed array implementation.
                 */
                Buffer.TYPED_ARRAY_SUPPORT = typedArraySupport()

                if (!Buffer.TYPED_ARRAY_SUPPORT && typeof console !== 'undefined' &&
                    typeof console.error === 'function') {
                    console.error(
                        'This browser lacks typed array (Uint8Array) support which is required by ' +
                        '`buffer` v5.x. Use `buffer` v4.x if you require old browser support.'
                    )
                }

                function typedArraySupport() {
                    // Can typed array instances can be augmented?
                    try {
                        var arr = new Uint8Array(1)
                        arr.__proto__ = {
                            __proto__: Uint8Array.prototype,
                            foo: function () {
                                return 42
                            }
                        }
                        return arr.foo() === 42
                    } catch (e) {
                        return false
                    }
                }

                Object.defineProperty(Buffer.prototype, 'parent', {
                    enumerable: true,
                    get: function () {
                        if (!Buffer.isBuffer(this)) return undefined
                        return this.buffer
                    }
                })

                Object.defineProperty(Buffer.prototype, 'offset', {
                    enumerable: true,
                    get: function () {
                        if (!Buffer.isBuffer(this)) return undefined
                        return this.byteOffset
                    }
                })

                function createBuffer(length) {
                    if (length > K_MAX_LENGTH) {
                        throw new RangeError('The value "' + length + '" is invalid for option "size"')
                    }
                    // Return an augmented `Uint8Array` instance
                    var buf = new Uint8Array(length)
                    buf.__proto__ = Buffer.prototype
                    return buf
                }

                /**
                 * The Buffer constructor returns instances of `Uint8Array` that have their
                 * prototype changed to `Buffer.prototype`. Furthermore, `Buffer` is a subclass of
                 * `Uint8Array`, so the returned instances will have all the node `Buffer` methods
                 * and the `Uint8Array` methods. Square bracket notation works as expected -- it
                 * returns a single octet.
                 *
                 * The `Uint8Array` prototype remains unmodified.
                 */

                function Buffer(arg, encodingOrOffset, length) {
                    // Common case.
                    if (typeof arg === 'number') {
                        if (typeof encodingOrOffset === 'string') {
                            throw new TypeError(
                                'The "string" argument must be of type string. Received type number'
                            )
                        }
                        return allocUnsafe(arg)
                    }
                    return from(arg, encodingOrOffset, length)
                }

                // Fix subarray() in ES2016. See: https://github.com/feross/buffer/pull/97
                if (typeof Symbol !== 'undefined' && Symbol.species != null &&
                    Buffer[Symbol.species] === Buffer) {
                    Object.defineProperty(Buffer, Symbol.species, {
                        value: null,
                        configurable: true,
                        enumerable: false,
                        writable: false
                    })
                }

                Buffer.poolSize = 8192 // not used by this implementation

                function from(value, encodingOrOffset, length) {
                    if (typeof value === 'string') {
                        return fromString(value, encodingOrOffset)
                    }

                    if (ArrayBuffer.isView(value)) {
                        return fromArrayLike(value)
                    }

                    if (value == null) {
                        throw TypeError(
                            'The first argument must be one of type string, Buffer, ArrayBuffer, Array, ' +
                            'or Array-like Object. Received type ' + (typeof value)
                        )
                    }

                    if (isInstance(value, ArrayBuffer) ||
                        (value && isInstance(value.buffer, ArrayBuffer))) {
                        return fromArrayBuffer(value, encodingOrOffset, length)
                    }

                    if (typeof value === 'number') {
                        throw new TypeError(
                            'The "value" argument must not be of type number. Received type number'
                        )
                    }

                    var valueOf = value.valueOf && value.valueOf()
                    if (valueOf != null && valueOf !== value) {
                        return Buffer.from(valueOf, encodingOrOffset, length)
                    }

                    var b = fromObject(value)
                    if (b) return b

                    if (typeof Symbol !== 'undefined' && Symbol.toPrimitive != null &&
                        typeof value[Symbol.toPrimitive] === 'function') {
                        return Buffer.from(
                            value[Symbol.toPrimitive]('string'), encodingOrOffset, length
                        )
                    }

                    throw new TypeError(
                        'The first argument must be one of type string, Buffer, ArrayBuffer, Array, ' +
                        'or Array-like Object. Received type ' + (typeof value)
                    )
                }

                /**
                 * Functionally equivalent to Buffer(arg, encoding) but throws a TypeError
                 * if value is a number.
                 * Buffer.from(str[, encoding])
                 * Buffer.from(array)
                 * Buffer.from(buffer)
                 * Buffer.from(arrayBuffer[, byteOffset[, length]])
                 **/
                Buffer.from = function (value, encodingOrOffset, length) {
                    return from(value, encodingOrOffset, length)
                }

                // Note: Change prototype *after* Buffer.from is defined to workaround Chrome bug:
                // https://github.com/feross/buffer/pull/148
                Buffer.prototype.__proto__ = Uint8Array.prototype
                Buffer.__proto__ = Uint8Array

                function assertSize(size) {
                    if (typeof size !== 'number') {
                        throw new TypeError('"size" argument must be of type number')
                    } else if (size < 0) {
                        throw new RangeError('The value "' + size + '" is invalid for option "size"')
                    }
                }

                function alloc(size, fill, encoding) {
                    assertSize(size)
                    if (size <= 0) {
                        return createBuffer(size)
                    }
                    if (fill !== undefined) {
                        // Only pay attention to encoding if it's a string. This
                        // prevents accidentally sending in a number that would
                        // be interpretted as a start offset.
                        return typeof encoding === 'string' ?
                            createBuffer(size).fill(fill, encoding) :
                            createBuffer(size).fill(fill)
                    }
                    return createBuffer(size)
                }

                /**
                 * Creates a new filled Buffer instance.
                 * alloc(size[, fill[, encoding]])
                 **/
                Buffer.alloc = function (size, fill, encoding) {
                    return alloc(size, fill, encoding)
                }

                function allocUnsafe(size) {
                    assertSize(size)
                    return createBuffer(size < 0 ? 0 : checked(size) | 0)
                }

                /**
                 * Equivalent to Buffer(num), by default creates a non-zero-filled Buffer instance.
                 * */
                Buffer.allocUnsafe = function (size) {
                    return allocUnsafe(size)
                }
                /**
                 * Equivalent to SlowBuffer(num), by default creates a non-zero-filled Buffer instance.
                 */
                Buffer.allocUnsafeSlow = function (size) {
                    return allocUnsafe(size)
                }

                function fromString(string, encoding) {
                    if (typeof encoding !== 'string' || encoding === '') {
                        encoding = 'utf8'
                    }

                    if (!Buffer.isEncoding(encoding)) {
                        throw new TypeError('Unknown encoding: ' + encoding)
                    }

                    var length = byteLength(string, encoding) | 0
                    var buf = createBuffer(length)

                    var actual = buf.write(string, encoding)

                    if (actual !== length) {
                        // Writing a hex string, for example, that contains invalid characters will
                        // cause everything after the first invalid character to be ignored. (e.g.
                        // 'abxxcd' will be treated as 'ab')
                        buf = buf.slice(0, actual)
                    }

                    return buf
                }

                function fromArrayLike(array) {
                    var length = array.length < 0 ? 0 : checked(array.length) | 0
                    var buf = createBuffer(length)
                    for (var i = 0; i < length; i += 1) {
                        buf[i] = array[i] & 255
                    }
                    return buf
                }

                function fromArrayBuffer(array, byteOffset, length) {
                    if (byteOffset < 0 || array.byteLength < byteOffset) {
                        throw new RangeError('"offset" is outside of buffer bounds')
                    }

                    if (array.byteLength < byteOffset + (length || 0)) {
                        throw new RangeError('"length" is outside of buffer bounds')
                    }

                    var buf
                    if (byteOffset === undefined && length === undefined) {
                        buf = new Uint8Array(array)
                    } else if (length === undefined) {
                        buf = new Uint8Array(array, byteOffset)
                    } else {
                        buf = new Uint8Array(array, byteOffset, length)
                    }

                    // Return an augmented `Uint8Array` instance
                    buf.__proto__ = Buffer.prototype
                    return buf
                }

                function fromObject(obj) {
                    if (Buffer.isBuffer(obj)) {
                        var len = checked(obj.length) | 0
                        var buf = createBuffer(len)

                        if (buf.length === 0) {
                            return buf
                        }

                        obj.copy(buf, 0, 0, len)
                        return buf
                    }

                    if (obj.length !== undefined) {
                        if (typeof obj.length !== 'number' || numberIsNaN(obj.length)) {
                            return createBuffer(0)
                        }
                        return fromArrayLike(obj)
                    }

                    if (obj.type === 'Buffer' && Array.isArray(obj.data)) {
                        return fromArrayLike(obj.data)
                    }
                }

                function checked(length) {
                    // Note: cannot use `length < K_MAX_LENGTH` here because that fails when
                    // length is NaN (which is otherwise coerced to zero.)
                    if (length >= K_MAX_LENGTH) {
                        throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
                            'size: 0x' + K_MAX_LENGTH.toString(16) + ' bytes')
                    }
                    return length | 0
                }

                function SlowBuffer(length) {
                    if (+length != length) { // eslint-disable-line eqeqeq
                        length = 0
                    }
                    return Buffer.alloc(+length)
                }

                Buffer.isBuffer = function isBuffer(b) {
                    return b != null && b._isBuffer === true &&
                        b !== Buffer.prototype // so Buffer.isBuffer(Buffer.prototype) will be false
                }

                Buffer.compare = function compare(a, b) {
                    if (isInstance(a, Uint8Array)) a = Buffer.from(a, a.offset, a.byteLength)
                    if (isInstance(b, Uint8Array)) b = Buffer.from(b, b.offset, b.byteLength)
                    if (!Buffer.isBuffer(a) || !Buffer.isBuffer(b)) {
                        throw new TypeError(
                            'The "buf1", "buf2" arguments must be one of type Buffer or Uint8Array'
                        )
                    }

                    if (a === b) return 0

                    var x = a.length
                    var y = b.length

                    for (var i = 0, len = Math.min(x, y); i < len; ++i) {
                        if (a[i] !== b[i]) {
                            x = a[i]
                            y = b[i]
                            break
                        }
                    }

                    if (x < y) return -1
                    if (y < x) return 1
                    return 0
                }

                Buffer.isEncoding = function isEncoding(encoding) {
                    switch (String(encoding).toLowerCase()) {
                        case 'hex':
                        case 'utf8':
                        case 'utf-8':
                        case 'ascii':
                        case 'latin1':
                        case 'binary':
                        case 'base64':
                        case 'ucs2':
                        case 'ucs-2':
                        case 'utf16le':
                        case 'utf-16le':
                            return true
                        default:
                            return false
                    }
                }

                Buffer.concat = function concat(list, length) {
                    if (!Array.isArray(list)) {
                        throw new TypeError('"list" argument must be an Array of Buffers')
                    }

                    if (list.length === 0) {
                        return Buffer.alloc(0)
                    }

                    var i
                    if (length === undefined) {
                        length = 0
                        for (i = 0; i < list.length; ++i) {
                            length += list[i].length
                        }
                    }

                    var buffer = Buffer.allocUnsafe(length)
                    var pos = 0
                    for (i = 0; i < list.length; ++i) {
                        var buf = list[i]
                        if (isInstance(buf, Uint8Array)) {
                            buf = Buffer.from(buf)
                        }
                        if (!Buffer.isBuffer(buf)) {
                            throw new TypeError('"list" argument must be an Array of Buffers')
                        }
                        buf.copy(buffer, pos)
                        pos += buf.length
                    }
                    return buffer
                }

                function byteLength(string, encoding) {
                    if (Buffer.isBuffer(string)) {
                        return string.length
                    }
                    if (ArrayBuffer.isView(string) || isInstance(string, ArrayBuffer)) {
                        return string.byteLength
                    }
                    if (typeof string !== 'string') {
                        throw new TypeError(
                            'The "string" argument must be one of type string, Buffer, or ArrayBuffer. ' +
                            'Received type ' + typeof string
                        )
                    }

                    var len = string.length
                    var mustMatch = (arguments.length > 2 && arguments[2] === true)
                    if (!mustMatch && len === 0) return 0

                    // Use a for loop to avoid recursion
                    var loweredCase = false
                    for (; ;) {
                        switch (encoding) {
                            case 'ascii':
                            case 'latin1':
                            case 'binary':
                                return len
                            case 'utf8':
                            case 'utf-8':
                                return utf8ToBytes(string).length
                            case 'ucs2':
                            case 'ucs-2':
                            case 'utf16le':
                            case 'utf-16le':
                                return len * 2
                            case 'hex':
                                return len >>> 1
                            case 'base64':
                                return base64ToBytes(string).length
                            default:
                                if (loweredCase) {
                                    return mustMatch ? -1 : utf8ToBytes(string).length // assume utf8
                                }
                                encoding = ('' + encoding).toLowerCase()
                                loweredCase = true
                        }
                    }
                }
                Buffer.byteLength = byteLength

                function slowToString(encoding, start, end) {
                    var loweredCase = false

                    // No need to verify that "this.length <= MAX_UINT32" since it's a read-only
                    // property of a typed array.

                    // This behaves neither like String nor Uint8Array in that we set start/end
                    // to their upper/lower bounds if the value passed is out of range.
                    // undefined is handled specially as per ECMA-262 6th Edition,
                    // Section 13.3.3.7 Runtime Semantics: KeyedBindingInitialization.
                    if (start === undefined || start < 0) {
                        start = 0
                    }
                    // Return early if start > this.length. Done here to prevent potential uint32
                    // coercion fail below.
                    if (start > this.length) {
                        return ''
                    }

                    if (end === undefined || end > this.length) {
                        end = this.length
                    }

                    if (end <= 0) {
                        return ''
                    }

                    // Force coersion to uint32. This will also coerce falsey/NaN values to 0.
                    end >>>= 0
                    start >>>= 0

                    if (end <= start) {
                        return ''
                    }

                    if (!encoding) encoding = 'utf8'

                    while (true) {
                        switch (encoding) {
                            case 'hex':
                                return hexSlice(this, start, end)

                            case 'utf8':
                            case 'utf-8':
                                return utf8Slice(this, start, end)

                            case 'ascii':
                                return asciiSlice(this, start, end)

                            case 'latin1':
                            case 'binary':
                                return latin1Slice(this, start, end)

                            case 'base64':
                                return base64Slice(this, start, end)

                            case 'ucs2':
                            case 'ucs-2':
                            case 'utf16le':
                            case 'utf-16le':
                                return utf16leSlice(this, start, end)

                            default:
                                if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
                                encoding = (encoding + '').toLowerCase()
                                loweredCase = true
                        }
                    }
                }

                // This property is used by `Buffer.isBuffer` (and the `is-buffer` npm package)
                // to detect a Buffer instance. It's not possible to use `instanceof Buffer`
                // reliably in a browserify context because there could be multiple different
                // copies of the 'buffer' package in use. This method works even for Buffer
                // instances that were created from another copy of the `buffer` package.
                // See: https://github.com/feross/buffer/issues/154
                Buffer.prototype._isBuffer = true

                function swap(b, n, m) {
                    var i = b[n]
                    b[n] = b[m]
                    b[m] = i
                }

                Buffer.prototype.swap16 = function swap16() {
                    var len = this.length
                    if (len % 2 !== 0) {
                        throw new RangeError('Buffer size must be a multiple of 16-bits')
                    }
                    for (var i = 0; i < len; i += 2) {
                        swap(this, i, i + 1)
                    }
                    return this
                }

                Buffer.prototype.swap32 = function swap32() {
                    var len = this.length
                    if (len % 4 !== 0) {
                        throw new RangeError('Buffer size must be a multiple of 32-bits')
                    }
                    for (var i = 0; i < len; i += 4) {
                        swap(this, i, i + 3)
                        swap(this, i + 1, i + 2)
                    }
                    return this
                }

                Buffer.prototype.swap64 = function swap64() {
                    var len = this.length
                    if (len % 8 !== 0) {
                        throw new RangeError('Buffer size must be a multiple of 64-bits')
                    }
                    for (var i = 0; i < len; i += 8) {
                        swap(this, i, i + 7)
                        swap(this, i + 1, i + 6)
                        swap(this, i + 2, i + 5)
                        swap(this, i + 3, i + 4)
                    }
                    return this
                }

                Buffer.prototype.toString = function toString() {
                    var length = this.length
                    if (length === 0) return ''
                    if (arguments.length === 0) return utf8Slice(this, 0, length)
                    return slowToString.apply(this, arguments)
                }

                Buffer.prototype.toLocaleString = Buffer.prototype.toString

                Buffer.prototype.equals = function equals(b) {
                    if (!Buffer.isBuffer(b)) throw new TypeError('Argument must be a Buffer')
                    if (this === b) return true
                    return Buffer.compare(this, b) === 0
                }

                Buffer.prototype.inspect = function inspect() {
                    var str = ''
                    var max = exports.INSPECT_MAX_BYTES
                    str = this.toString('hex', 0, max).replace(/(.{2})/g, '$1 ').trim()
                    if (this.length > max) str += ' ... '
                    return '<Buffer ' + str + '>'
                }

                Buffer.prototype.compare = function compare(target, start, end, thisStart, thisEnd) {
                    if (isInstance(target, Uint8Array)) {
                        target = Buffer.from(target, target.offset, target.byteLength)
                    }
                    if (!Buffer.isBuffer(target)) {
                        throw new TypeError(
                            'The "target" argument must be one of type Buffer or Uint8Array. ' +
                            'Received type ' + (typeof target)
                        )
                    }

                    if (start === undefined) {
                        start = 0
                    }
                    if (end === undefined) {
                        end = target ? target.length : 0
                    }
                    if (thisStart === undefined) {
                        thisStart = 0
                    }
                    if (thisEnd === undefined) {
                        thisEnd = this.length
                    }

                    if (start < 0 || end > target.length || thisStart < 0 || thisEnd > this.length) {
                        throw new RangeError('out of range index')
                    }

                    if (thisStart >= thisEnd && start >= end) {
                        return 0
                    }
                    if (thisStart >= thisEnd) {
                        return -1
                    }
                    if (start >= end) {
                        return 1
                    }

                    start >>>= 0
                    end >>>= 0
                    thisStart >>>= 0
                    thisEnd >>>= 0

                    if (this === target) return 0

                    var x = thisEnd - thisStart
                    var y = end - start
                    var len = Math.min(x, y)

                    var thisCopy = this.slice(thisStart, thisEnd)
                    var targetCopy = target.slice(start, end)

                    for (var i = 0; i < len; ++i) {
                        if (thisCopy[i] !== targetCopy[i]) {
                            x = thisCopy[i]
                            y = targetCopy[i]
                            break
                        }
                    }

                    if (x < y) return -1
                    if (y < x) return 1
                    return 0
                }

                // Finds either the first index of `val` in `buffer` at offset >= `byteOffset`,
                // OR the last index of `val` in `buffer` at offset <= `byteOffset`.
                //
                // Arguments:
                // - buffer - a Buffer to search
                // - val - a string, Buffer, or number
                // - byteOffset - an index into `buffer`; will be clamped to an int32
                // - encoding - an optional encoding, relevant is val is a string
                // - dir - true for indexOf, false for lastIndexOf
                function bidirectionalIndexOf(buffer, val, byteOffset, encoding, dir) {
                    // Empty buffer means no match
                    if (buffer.length === 0) return -1

                    // Normalize byteOffset
                    if (typeof byteOffset === 'string') {
                        encoding = byteOffset
                        byteOffset = 0
                    } else if (byteOffset > 0x7fffffff) {
                        byteOffset = 0x7fffffff
                    } else if (byteOffset < -0x80000000) {
                        byteOffset = -0x80000000
                    }
                    byteOffset = +byteOffset // Coerce to Number.
                    if (numberIsNaN(byteOffset)) {
                        // byteOffset: it it's undefined, null, NaN, "foo", etc, search whole buffer
                        byteOffset = dir ? 0 : (buffer.length - 1)
                    }

                    // Normalize byteOffset: negative offsets start from the end of the buffer
                    if (byteOffset < 0) byteOffset = buffer.length + byteOffset
                    if (byteOffset >= buffer.length) {
                        if (dir) return -1
                        else byteOffset = buffer.length - 1
                    } else if (byteOffset < 0) {
                        if (dir) byteOffset = 0
                        else return -1
                    }

                    // Normalize val
                    if (typeof val === 'string') {
                        val = Buffer.from(val, encoding)
                    }

                    // Finally, search either indexOf (if dir is true) or lastIndexOf
                    if (Buffer.isBuffer(val)) {
                        // Special case: looking for empty string/buffer always fails
                        if (val.length === 0) {
                            return -1
                        }
                        return arrayIndexOf(buffer, val, byteOffset, encoding, dir)
                    } else if (typeof val === 'number') {
                        val = val & 0xFF // Search for a byte value [0-255]
                        if (typeof Uint8Array.prototype.indexOf === 'function') {
                            if (dir) {
                                return Uint8Array.prototype.indexOf.call(buffer, val, byteOffset)
                            } else {
                                return Uint8Array.prototype.lastIndexOf.call(buffer, val, byteOffset)
                            }
                        }
                        return arrayIndexOf(buffer, [val], byteOffset, encoding, dir)
                    }

                    throw new TypeError('val must be string, number or Buffer')
                }

                function arrayIndexOf(arr, val, byteOffset, encoding, dir) {
                    var indexSize = 1
                    var arrLength = arr.length
                    var valLength = val.length

                    if (encoding !== undefined) {
                        encoding = String(encoding).toLowerCase()
                        if (encoding === 'ucs2' || encoding === 'ucs-2' ||
                            encoding === 'utf16le' || encoding === 'utf-16le') {
                            if (arr.length < 2 || val.length < 2) {
                                return -1
                            }
                            indexSize = 2
                            arrLength /= 2
                            valLength /= 2
                            byteOffset /= 2
                        }
                    }

                    function read(buf, i) {
                        if (indexSize === 1) {
                            return buf[i]
                        } else {
                            return buf.readUInt16BE(i * indexSize)
                        }
                    }

                    var i
                    if (dir) {
                        var foundIndex = -1
                        for (i = byteOffset; i < arrLength; i++) {
                            if (read(arr, i) === read(val, foundIndex === -1 ? 0 : i - foundIndex)) {
                                if (foundIndex === -1) foundIndex = i
                                if (i - foundIndex + 1 === valLength) return foundIndex * indexSize
                            } else {
                                if (foundIndex !== -1) i -= i - foundIndex
                                foundIndex = -1
                            }
                        }
                    } else {
                        if (byteOffset + valLength > arrLength) byteOffset = arrLength - valLength
                        for (i = byteOffset; i >= 0; i--) {
                            var found = true
                            for (var j = 0; j < valLength; j++) {
                                if (read(arr, i + j) !== read(val, j)) {
                                    found = false
                                    break
                                }
                            }
                            if (found) return i
                        }
                    }

                    return -1
                }

                Buffer.prototype.includes = function includes(val, byteOffset, encoding) {
                    return this.indexOf(val, byteOffset, encoding) !== -1
                }

                Buffer.prototype.indexOf = function indexOf(val, byteOffset, encoding) {
                    return bidirectionalIndexOf(this, val, byteOffset, encoding, true)
                }

                Buffer.prototype.lastIndexOf = function lastIndexOf(val, byteOffset, encoding) {
                    return bidirectionalIndexOf(this, val, byteOffset, encoding, false)
                }

                function hexWrite(buf, string, offset, length) {
                    offset = Number(offset) || 0
                    var remaining = buf.length - offset
                    if (!length) {
                        length = remaining
                    } else {
                        length = Number(length)
                        if (length > remaining) {
                            length = remaining
                        }
                    }

                    var strLen = string.length

                    if (length > strLen / 2) {
                        length = strLen / 2
                    }
                    for (var i = 0; i < length; ++i) {
                        var parsed = parseInt(string.substr(i * 2, 2), 16)
                        if (numberIsNaN(parsed)) return i
                        buf[offset + i] = parsed
                    }
                    return i
                }

                function utf8Write(buf, string, offset, length) {
                    return blitBuffer(utf8ToBytes(string, buf.length - offset), buf, offset, length)
                }

                function asciiWrite(buf, string, offset, length) {
                    return blitBuffer(asciiToBytes(string), buf, offset, length)
                }

                function latin1Write(buf, string, offset, length) {
                    return asciiWrite(buf, string, offset, length)
                }

                function base64Write(buf, string, offset, length) {
                    return blitBuffer(base64ToBytes(string), buf, offset, length)
                }

                function ucs2Write(buf, string, offset, length) {
                    return blitBuffer(utf16leToBytes(string, buf.length - offset), buf, offset, length)
                }

                Buffer.prototype.write = function write(string, offset, length, encoding) {
                    // Buffer#write(string)
                    if (offset === undefined) {
                        encoding = 'utf8'
                        length = this.length
                        offset = 0
                        // Buffer#write(string, encoding)
                    } else if (length === undefined && typeof offset === 'string') {
                        encoding = offset
                        length = this.length
                        offset = 0
                        // Buffer#write(string, offset[, length][, encoding])
                    } else if (isFinite(offset)) {
                        offset = offset >>> 0
                        if (isFinite(length)) {
                            length = length >>> 0
                            if (encoding === undefined) encoding = 'utf8'
                        } else {
                            encoding = length
                            length = undefined
                        }
                    } else {
                        throw new Error(
                            'Buffer.write(string, encoding, offset[, length]) is no longer supported'
                        )
                    }

                    var remaining = this.length - offset
                    if (length === undefined || length > remaining) length = remaining

                    if ((string.length > 0 && (length < 0 || offset < 0)) || offset > this.length) {
                        throw new RangeError('Attempt to write outside buffer bounds')
                    }

                    if (!encoding) encoding = 'utf8'

                    var loweredCase = false
                    for (; ;) {
                        switch (encoding) {
                            case 'hex':
                                return hexWrite(this, string, offset, length)

                            case 'utf8':
                            case 'utf-8':
                                return utf8Write(this, string, offset, length)

                            case 'ascii':
                                return asciiWrite(this, string, offset, length)

                            case 'latin1':
                            case 'binary':
                                return latin1Write(this, string, offset, length)

                            case 'base64':
                                // Warning: maxLength not taken into account in base64Write
                                return base64Write(this, string, offset, length)

                            case 'ucs2':
                            case 'ucs-2':
                            case 'utf16le':
                            case 'utf-16le':
                                return ucs2Write(this, string, offset, length)

                            default:
                                if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
                                encoding = ('' + encoding).toLowerCase()
                                loweredCase = true
                        }
                    }
                }

                Buffer.prototype.toJSON = function toJSON() {
                    return {
                        type: 'Buffer',
                        data: Array.prototype.slice.call(this._arr || this, 0)
                    }
                }

                function base64Slice(buf, start, end) {
                    if (start === 0 && end === buf.length) {
                        return base64.fromByteArray(buf)
                    } else {
                        return base64.fromByteArray(buf.slice(start, end))
                    }
                }

                function utf8Slice(buf, start, end) {
                    end = Math.min(buf.length, end)
                    var res = []

                    var i = start
                    while (i < end) {
                        var firstByte = buf[i]
                        var codePoint = null
                        var bytesPerSequence = (firstByte > 0xEF) ? 4 :
                            (firstByte > 0xDF) ? 3 :
                                (firstByte > 0xBF) ? 2 :
                                    1

                        if (i + bytesPerSequence <= end) {
                            var secondByte, thirdByte, fourthByte, tempCodePoint

                            switch (bytesPerSequence) {
                                case 1:
                                    if (firstByte < 0x80) {
                                        codePoint = firstByte
                                    }
                                    break
                                case 2:
                                    secondByte = buf[i + 1]
                                    if ((secondByte & 0xC0) === 0x80) {
                                        tempCodePoint = (firstByte & 0x1F) << 0x6 | (secondByte & 0x3F)
                                        if (tempCodePoint > 0x7F) {
                                            codePoint = tempCodePoint
                                        }
                                    }
                                    break
                                case 3:
                                    secondByte = buf[i + 1]
                                    thirdByte = buf[i + 2]
                                    if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80) {
                                        tempCodePoint = (firstByte & 0xF) << 0xC | (secondByte & 0x3F) << 0x6 | (thirdByte & 0x3F)
                                        if (tempCodePoint > 0x7FF && (tempCodePoint < 0xD800 || tempCodePoint > 0xDFFF)) {
                                            codePoint = tempCodePoint
                                        }
                                    }
                                    break
                                case 4:
                                    secondByte = buf[i + 1]
                                    thirdByte = buf[i + 2]
                                    fourthByte = buf[i + 3]
                                    if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80 && (fourthByte & 0xC0) === 0x80) {
                                        tempCodePoint = (firstByte & 0xF) << 0x12 | (secondByte & 0x3F) << 0xC | (thirdByte & 0x3F) << 0x6 | (fourthByte & 0x3F)
                                        if (tempCodePoint > 0xFFFF && tempCodePoint < 0x110000) {
                                            codePoint = tempCodePoint
                                        }
                                    }
                            }
                        }

                        if (codePoint === null) {
                            // we did not generate a valid codePoint so insert a
                            // replacement char (U+FFFD) and advance only 1 byte
                            codePoint = 0xFFFD
                            bytesPerSequence = 1
                        } else if (codePoint > 0xFFFF) {
                            // encode to utf16 (surrogate pair dance)
                            codePoint -= 0x10000
                            res.push(codePoint >>> 10 & 0x3FF | 0xD800)
                            codePoint = 0xDC00 | codePoint & 0x3FF
                        }

                        res.push(codePoint)
                        i += bytesPerSequence
                    }

                    return decodeCodePointsArray(res)
                }

                // Based on http://stackoverflow.com/a/22747272/680742, the browser with
                // the lowest limit is Chrome, with 0x10000 args.
                // We go 1 magnitude less, for safety
                var MAX_ARGUMENTS_LENGTH = 0x1000

                function decodeCodePointsArray(codePoints) {
                    var len = codePoints.length
                    if (len <= MAX_ARGUMENTS_LENGTH) {
                        return String.fromCharCode.apply(String, codePoints) // avoid extra slice()
                    }

                    // Decode in chunks to avoid "call stack size exceeded".
                    var res = ''
                    var i = 0
                    while (i < len) {
                        res += String.fromCharCode.apply(
                            String,
                            codePoints.slice(i, i += MAX_ARGUMENTS_LENGTH)
                        )
                    }
                    return res
                }

                function asciiSlice(buf, start, end) {
                    var ret = ''
                    end = Math.min(buf.length, end)

                    for (var i = start; i < end; ++i) {
                        ret += String.fromCharCode(buf[i] & 0x7F)
                    }
                    return ret
                }

                function latin1Slice(buf, start, end) {
                    var ret = ''
                    end = Math.min(buf.length, end)

                    for (var i = start; i < end; ++i) {
                        ret += String.fromCharCode(buf[i])
                    }
                    return ret
                }

                function hexSlice(buf, start, end) {
                    var len = buf.length

                    if (!start || start < 0) start = 0
                    if (!end || end < 0 || end > len) end = len

                    var out = ''
                    for (var i = start; i < end; ++i) {
                        out += toHex(buf[i])
                    }
                    return out
                }

                function utf16leSlice(buf, start, end) {
                    var bytes = buf.slice(start, end)
                    var res = ''
                    for (var i = 0; i < bytes.length; i += 2) {
                        res += String.fromCharCode(bytes[i] + (bytes[i + 1] * 256))
                    }
                    return res
                }

                Buffer.prototype.slice = function slice(start, end) {
                    var len = this.length
                    start = ~~start
                    end = end === undefined ? len : ~~end

                    if (start < 0) {
                        start += len
                        if (start < 0) start = 0
                    } else if (start > len) {
                        start = len
                    }

                    if (end < 0) {
                        end += len
                        if (end < 0) end = 0
                    } else if (end > len) {
                        end = len
                    }

                    if (end < start) end = start

                    var newBuf = this.subarray(start, end)
                    // Return an augmented `Uint8Array` instance
                    newBuf.__proto__ = Buffer.prototype
                    return newBuf
                }

                /*
                 * Need to make sure that buffer isn't trying to write out of bounds.
                 */
                function checkOffset(offset, ext, length) {
                    if ((offset % 1) !== 0 || offset < 0) throw new RangeError('offset is not uint')
                    if (offset + ext > length) throw new RangeError('Trying to access beyond buffer length')
                }

                Buffer.prototype.readUIntLE = function readUIntLE(offset, byteLength, noAssert) {
                    offset = offset >>> 0
                    byteLength = byteLength >>> 0
                    if (!noAssert) checkOffset(offset, byteLength, this.length)

                    var val = this[offset]
                    var mul = 1
                    var i = 0
                    while (++i < byteLength && (mul *= 0x100)) {
                        val += this[offset + i] * mul
                    }

                    return val
                }

                Buffer.prototype.readUIntBE = function readUIntBE(offset, byteLength, noAssert) {
                    offset = offset >>> 0
                    byteLength = byteLength >>> 0
                    if (!noAssert) {
                        checkOffset(offset, byteLength, this.length)
                    }

                    var val = this[offset + --byteLength]
                    var mul = 1
                    while (byteLength > 0 && (mul *= 0x100)) {
                        val += this[offset + --byteLength] * mul
                    }

                    return val
                }

                Buffer.prototype.readUInt8 = function readUInt8(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 1, this.length)
                    return this[offset]
                }

                Buffer.prototype.readUInt16LE = function readUInt16LE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 2, this.length)
                    return this[offset] | (this[offset + 1] << 8)
                }

                Buffer.prototype.readUInt16BE = function readUInt16BE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 2, this.length)
                    return (this[offset] << 8) | this[offset + 1]
                }

                Buffer.prototype.readUInt32LE = function readUInt32LE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 4, this.length)

                    return ((this[offset]) |
                        (this[offset + 1] << 8) |
                        (this[offset + 2] << 16)) +
                        (this[offset + 3] * 0x1000000)
                }

                Buffer.prototype.readUInt32BE = function readUInt32BE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 4, this.length)

                    return (this[offset] * 0x1000000) +
                        ((this[offset + 1] << 16) |
                            (this[offset + 2] << 8) |
                            this[offset + 3])
                }

                Buffer.prototype.readIntLE = function readIntLE(offset, byteLength, noAssert) {
                    offset = offset >>> 0
                    byteLength = byteLength >>> 0
                    if (!noAssert) checkOffset(offset, byteLength, this.length)

                    var val = this[offset]
                    var mul = 1
                    var i = 0
                    while (++i < byteLength && (mul *= 0x100)) {
                        val += this[offset + i] * mul
                    }
                    mul *= 0x80

                    if (val >= mul) val -= Math.pow(2, 8 * byteLength)

                    return val
                }

                Buffer.prototype.readIntBE = function readIntBE(offset, byteLength, noAssert) {
                    offset = offset >>> 0
                    byteLength = byteLength >>> 0
                    if (!noAssert) checkOffset(offset, byteLength, this.length)

                    var i = byteLength
                    var mul = 1
                    var val = this[offset + --i]
                    while (i > 0 && (mul *= 0x100)) {
                        val += this[offset + --i] * mul
                    }
                    mul *= 0x80

                    if (val >= mul) val -= Math.pow(2, 8 * byteLength)

                    return val
                }

                Buffer.prototype.readInt8 = function readInt8(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 1, this.length)
                    if (!(this[offset] & 0x80)) return (this[offset])
                    return ((0xff - this[offset] + 1) * -1)
                }

                Buffer.prototype.readInt16LE = function readInt16LE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 2, this.length)
                    var val = this[offset] | (this[offset + 1] << 8)
                    return (val & 0x8000) ? val | 0xFFFF0000 : val
                }

                Buffer.prototype.readInt16BE = function readInt16BE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 2, this.length)
                    var val = this[offset + 1] | (this[offset] << 8)
                    return (val & 0x8000) ? val | 0xFFFF0000 : val
                }

                Buffer.prototype.readInt32LE = function readInt32LE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 4, this.length)

                    return (this[offset]) |
                        (this[offset + 1] << 8) |
                        (this[offset + 2] << 16) |
                        (this[offset + 3] << 24)
                }

                Buffer.prototype.readInt32BE = function readInt32BE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 4, this.length)

                    return (this[offset] << 24) |
                        (this[offset + 1] << 16) |
                        (this[offset + 2] << 8) |
                        (this[offset + 3])
                }

                Buffer.prototype.readFloatLE = function readFloatLE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 4, this.length)
                    return ieee754.read(this, offset, true, 23, 4)
                }

                Buffer.prototype.readFloatBE = function readFloatBE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 4, this.length)
                    return ieee754.read(this, offset, false, 23, 4)
                }

                Buffer.prototype.readDoubleLE = function readDoubleLE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 8, this.length)
                    return ieee754.read(this, offset, true, 52, 8)
                }

                Buffer.prototype.readDoubleBE = function readDoubleBE(offset, noAssert) {
                    offset = offset >>> 0
                    if (!noAssert) checkOffset(offset, 8, this.length)
                    return ieee754.read(this, offset, false, 52, 8)
                }

                function checkInt(buf, value, offset, ext, max, min) {
                    if (!Buffer.isBuffer(buf)) throw new TypeError('"buffer" argument must be a Buffer instance')
                    if (value > max || value < min) throw new RangeError('"value" argument is out of bounds')
                    if (offset + ext > buf.length) throw new RangeError('Index out of range')
                }

                Buffer.prototype.writeUIntLE = function writeUIntLE(value, offset, byteLength, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    byteLength = byteLength >>> 0
                    if (!noAssert) {
                        var maxBytes = Math.pow(2, 8 * byteLength) - 1
                        checkInt(this, value, offset, byteLength, maxBytes, 0)
                    }

                    var mul = 1
                    var i = 0
                    this[offset] = value & 0xFF
                    while (++i < byteLength && (mul *= 0x100)) {
                        this[offset + i] = (value / mul) & 0xFF
                    }

                    return offset + byteLength
                }

                Buffer.prototype.writeUIntBE = function writeUIntBE(value, offset, byteLength, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    byteLength = byteLength >>> 0
                    if (!noAssert) {
                        var maxBytes = Math.pow(2, 8 * byteLength) - 1
                        checkInt(this, value, offset, byteLength, maxBytes, 0)
                    }

                    var i = byteLength - 1
                    var mul = 1
                    this[offset + i] = value & 0xFF
                    while (--i >= 0 && (mul *= 0x100)) {
                        this[offset + i] = (value / mul) & 0xFF
                    }

                    return offset + byteLength
                }

                Buffer.prototype.writeUInt8 = function writeUInt8(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 1, 0xff, 0)
                    this[offset] = (value & 0xff)
                    return offset + 1
                }

                Buffer.prototype.writeUInt16LE = function writeUInt16LE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
                    this[offset] = (value & 0xff)
                    this[offset + 1] = (value >>> 8)
                    return offset + 2
                }

                Buffer.prototype.writeUInt16BE = function writeUInt16BE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
                    this[offset] = (value >>> 8)
                    this[offset + 1] = (value & 0xff)
                    return offset + 2
                }

                Buffer.prototype.writeUInt32LE = function writeUInt32LE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
                    this[offset + 3] = (value >>> 24)
                    this[offset + 2] = (value >>> 16)
                    this[offset + 1] = (value >>> 8)
                    this[offset] = (value & 0xff)
                    return offset + 4
                }

                Buffer.prototype.writeUInt32BE = function writeUInt32BE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
                    this[offset] = (value >>> 24)
                    this[offset + 1] = (value >>> 16)
                    this[offset + 2] = (value >>> 8)
                    this[offset + 3] = (value & 0xff)
                    return offset + 4
                }

                Buffer.prototype.writeIntLE = function writeIntLE(value, offset, byteLength, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) {
                        var limit = Math.pow(2, (8 * byteLength) - 1)

                        checkInt(this, value, offset, byteLength, limit - 1, -limit)
                    }

                    var i = 0
                    var mul = 1
                    var sub = 0
                    this[offset] = value & 0xFF
                    while (++i < byteLength && (mul *= 0x100)) {
                        if (value < 0 && sub === 0 && this[offset + i - 1] !== 0) {
                            sub = 1
                        }
                        this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
                    }

                    return offset + byteLength
                }

                Buffer.prototype.writeIntBE = function writeIntBE(value, offset, byteLength, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) {
                        var limit = Math.pow(2, (8 * byteLength) - 1)

                        checkInt(this, value, offset, byteLength, limit - 1, -limit)
                    }

                    var i = byteLength - 1
                    var mul = 1
                    var sub = 0
                    this[offset + i] = value & 0xFF
                    while (--i >= 0 && (mul *= 0x100)) {
                        if (value < 0 && sub === 0 && this[offset + i + 1] !== 0) {
                            sub = 1
                        }
                        this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
                    }

                    return offset + byteLength
                }

                Buffer.prototype.writeInt8 = function writeInt8(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 1, 0x7f, -0x80)
                    if (value < 0) value = 0xff + value + 1
                    this[offset] = (value & 0xff)
                    return offset + 1
                }

                Buffer.prototype.writeInt16LE = function writeInt16LE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
                    this[offset] = (value & 0xff)
                    this[offset + 1] = (value >>> 8)
                    return offset + 2
                }

                Buffer.prototype.writeInt16BE = function writeInt16BE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
                    this[offset] = (value >>> 8)
                    this[offset + 1] = (value & 0xff)
                    return offset + 2
                }

                Buffer.prototype.writeInt32LE = function writeInt32LE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
                    this[offset] = (value & 0xff)
                    this[offset + 1] = (value >>> 8)
                    this[offset + 2] = (value >>> 16)
                    this[offset + 3] = (value >>> 24)
                    return offset + 4
                }

                Buffer.prototype.writeInt32BE = function writeInt32BE(value, offset, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
                    if (value < 0) value = 0xffffffff + value + 1
                    this[offset] = (value >>> 24)
                    this[offset + 1] = (value >>> 16)
                    this[offset + 2] = (value >>> 8)
                    this[offset + 3] = (value & 0xff)
                    return offset + 4
                }

                function checkIEEE754(buf, value, offset, ext, max, min) {
                    if (offset + ext > buf.length) throw new RangeError('Index out of range')
                    if (offset < 0) throw new RangeError('Index out of range')
                }

                function writeFloat(buf, value, offset, littleEndian, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) {
                        checkIEEE754(buf, value, offset, 4, 3.4028234663852886e+38, -3.4028234663852886e+38)
                    }
                    ieee754.write(buf, value, offset, littleEndian, 23, 4)
                    return offset + 4
                }

                Buffer.prototype.writeFloatLE = function writeFloatLE(value, offset, noAssert) {
                    return writeFloat(this, value, offset, true, noAssert)
                }

                Buffer.prototype.writeFloatBE = function writeFloatBE(value, offset, noAssert) {
                    return writeFloat(this, value, offset, false, noAssert)
                }

                function writeDouble(buf, value, offset, littleEndian, noAssert) {
                    value = +value
                    offset = offset >>> 0
                    if (!noAssert) {
                        checkIEEE754(buf, value, offset, 8, 1.7976931348623157E+308, -1.7976931348623157E+308)
                    }
                    ieee754.write(buf, value, offset, littleEndian, 52, 8)
                    return offset + 8
                }

                Buffer.prototype.writeDoubleLE = function writeDoubleLE(value, offset, noAssert) {
                    return writeDouble(this, value, offset, true, noAssert)
                }

                Buffer.prototype.writeDoubleBE = function writeDoubleBE(value, offset, noAssert) {
                    return writeDouble(this, value, offset, false, noAssert)
                }

                // copy(targetBuffer, targetStart=0, sourceStart=0, sourceEnd=buffer.length)
                Buffer.prototype.copy = function copy(target, targetStart, start, end) {
                    if (!Buffer.isBuffer(target)) throw new TypeError('argument should be a Buffer')
                    if (!start) start = 0
                    if (!end && end !== 0) end = this.length
                    if (targetStart >= target.length) targetStart = target.length
                    if (!targetStart) targetStart = 0
                    if (end > 0 && end < start) end = start

                    // Copy 0 bytes; we're done
                    if (end === start) return 0
                    if (target.length === 0 || this.length === 0) return 0

                    // Fatal error conditions
                    if (targetStart < 0) {
                        throw new RangeError('targetStart out of bounds')
                    }
                    if (start < 0 || start >= this.length) throw new RangeError('Index out of range')
                    if (end < 0) throw new RangeError('sourceEnd out of bounds')

                    // Are we oob?
                    if (end > this.length) end = this.length
                    if (target.length - targetStart < end - start) {
                        end = target.length - targetStart + start
                    }

                    var len = end - start

                    if (this === target && typeof Uint8Array.prototype.copyWithin === 'function') {
                        // Use built-in when available, missing from IE11
                        this.copyWithin(targetStart, start, end)
                    } else if (this === target && start < targetStart && targetStart < end) {
                        // descending copy from end
                        for (var i = len - 1; i >= 0; --i) {
                            target[i + targetStart] = this[i + start]
                        }
                    } else {
                        Uint8Array.prototype.set.call(
                            target,
                            this.subarray(start, end),
                            targetStart
                        )
                    }

                    return len
                }

                // Usage:
                //    buffer.fill(number[, offset[, end]])
                //    buffer.fill(buffer[, offset[, end]])
                //    buffer.fill(string[, offset[, end]][, encoding])
                Buffer.prototype.fill = function fill(val, start, end, encoding) {
                    // Handle string cases:
                    if (typeof val === 'string') {
                        if (typeof start === 'string') {
                            encoding = start
                            start = 0
                            end = this.length
                        } else if (typeof end === 'string') {
                            encoding = end
                            end = this.length
                        }
                        if (encoding !== undefined && typeof encoding !== 'string') {
                            throw new TypeError('encoding must be a string')
                        }
                        if (typeof encoding === 'string' && !Buffer.isEncoding(encoding)) {
                            throw new TypeError('Unknown encoding: ' + encoding)
                        }
                        if (val.length === 1) {
                            var code = val.charCodeAt(0)
                            if ((encoding === 'utf8' && code < 128) ||
                                encoding === 'latin1') {
                                // Fast path: If `val` fits into a single byte, use that numeric value.
                                val = code
                            }
                        }
                    } else if (typeof val === 'number') {
                        val = val & 255
                    }

                    // Invalid ranges are not set to a default, so can range check early.
                    if (start < 0 || this.length < start || this.length < end) {
                        throw new RangeError('Out of range index')
                    }

                    if (end <= start) {
                        return this
                    }

                    start = start >>> 0
                    end = end === undefined ? this.length : end >>> 0

                    if (!val) val = 0

                    var i
                    if (typeof val === 'number') {
                        for (i = start; i < end; ++i) {
                            this[i] = val
                        }
                    } else {
                        var bytes = Buffer.isBuffer(val) ?
                            val :
                            Buffer.from(val, encoding)
                        var len = bytes.length
                        if (len === 0) {
                            throw new TypeError('The value "' + val +
                                '" is invalid for argument "value"')
                        }
                        for (i = 0; i < end - start; ++i) {
                            this[i + start] = bytes[i % len]
                        }
                    }

                    return this
                }

                // HELPER FUNCTIONS
                // ================

                var INVALID_BASE64_RE = /[^+/0-9A-Za-z-_]/g

                function base64clean(str) {
                    // Node takes equal signs as end of the Base64 encoding
                    str = str.split('=')[0]
                    // Node strips out invalid characters like \n and \t from the string, base64-js does not
                    str = str.trim().replace(INVALID_BASE64_RE, '')
                    // Node converts strings with length < 2 to ''
                    if (str.length < 2) return ''
                    // Node allows for non-padded base64 strings (missing trailing ===), base64-js does not
                    while (str.length % 4 !== 0) {
                        str = str + '='
                    }
                    return str
                }

                function toHex(n) {
                    if (n < 16) return '0' + n.toString(16)
                    return n.toString(16)
                }

                function utf8ToBytes(string, units) {
                    units = units || Infinity
                    var codePoint
                    var length = string.length
                    var leadSurrogate = null
                    var bytes = []

                    for (var i = 0; i < length; ++i) {
                        codePoint = string.charCodeAt(i)

                        // is surrogate component
                        if (codePoint > 0xD7FF && codePoint < 0xE000) {
                            // last char was a lead
                            if (!leadSurrogate) {
                                // no lead yet
                                if (codePoint > 0xDBFF) {
                                    // unexpected trail
                                    if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
                                    continue
                                } else if (i + 1 === length) {
                                    // unpaired lead
                                    if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
                                    continue
                                }

                                // valid lead
                                leadSurrogate = codePoint

                                continue
                            }

                            // 2 leads in a row
                            if (codePoint < 0xDC00) {
                                if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
                                leadSurrogate = codePoint
                                continue
                            }

                            // valid surrogate pair
                            codePoint = (leadSurrogate - 0xD800 << 10 | codePoint - 0xDC00) + 0x10000
                        } else if (leadSurrogate) {
                            // valid bmp char, but last char was a lead
                            if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
                        }

                        leadSurrogate = null

                        // encode utf8
                        if (codePoint < 0x80) {
                            if ((units -= 1) < 0) break
                            bytes.push(codePoint)
                        } else if (codePoint < 0x800) {
                            if ((units -= 2) < 0) break
                            bytes.push(
                                codePoint >> 0x6 | 0xC0,
                                codePoint & 0x3F | 0x80
                            )
                        } else if (codePoint < 0x10000) {
                            if ((units -= 3) < 0) break
                            bytes.push(
                                codePoint >> 0xC | 0xE0,
                                codePoint >> 0x6 & 0x3F | 0x80,
                                codePoint & 0x3F | 0x80
                            )
                        } else if (codePoint < 0x110000) {
                            if ((units -= 4) < 0) break
                            bytes.push(
                                codePoint >> 0x12 | 0xF0,
                                codePoint >> 0xC & 0x3F | 0x80,
                                codePoint >> 0x6 & 0x3F | 0x80,
                                codePoint & 0x3F | 0x80
                            )
                        } else {
                            throw new Error('Invalid code point')
                        }
                    }

                    return bytes
                }

                function asciiToBytes(str) {
                    var byteArray = []
                    for (var i = 0; i < str.length; ++i) {
                        // Node's code seems to be doing this and not & 0x7F..
                        byteArray.push(str.charCodeAt(i) & 0xFF)
                    }
                    return byteArray
                }

                function utf16leToBytes(str, units) {
                    var c, hi, lo
                    var byteArray = []
                    for (var i = 0; i < str.length; ++i) {
                        if ((units -= 2) < 0) break

                        c = str.charCodeAt(i)
                        hi = c >> 8
                        lo = c % 256
                        byteArray.push(lo)
                        byteArray.push(hi)
                    }

                    return byteArray
                }

                function base64ToBytes(str) {
                    return base64.toByteArray(base64clean(str))
                }

                function blitBuffer(src, dst, offset, length) {
                    for (var i = 0; i < length; ++i) {
                        if ((i + offset >= dst.length) || (i >= src.length)) break
                        dst[i + offset] = src[i]
                    }
                    return i
                }

                // ArrayBuffer or Uint8Array objects from other contexts (i.e. iframes) do not pass
                // the `instanceof` check but they should be treated as of that type.
                // See: https://github.com/feross/buffer/issues/166
                function isInstance(obj, type) {
                    return obj instanceof type ||
                        (obj != null && obj.constructor != null && obj.constructor.name != null &&
                            obj.constructor.name === type.name)
                }

                function numberIsNaN(obj) {
                    // For IE11 support
                    return obj !== obj // eslint-disable-line no-self-compare
                }

            }).call(this)
        }).call(this, require("buffer").Buffer)
    }, {
        "base64-js": 5,
        "buffer": 6,
        "ieee754": 7
    }],
    7: [function (require, module, exports) {
        /*! ieee754. BSD-3-Clause License. Feross Aboukhadijeh <https://feross.org/opensource> */
        exports.read = function (buffer, offset, isLE, mLen, nBytes) {
            var e, m
            var eLen = (nBytes * 8) - mLen - 1
            var eMax = (1 << eLen) - 1
            var eBias = eMax >> 1
            var nBits = -7
            var i = isLE ? (nBytes - 1) : 0
            var d = isLE ? -1 : 1
            var s = buffer[offset + i]

            i += d

            e = s & ((1 << (-nBits)) - 1)
            s >>= (-nBits)
            nBits += eLen
            for (; nBits > 0; e = (e * 256) + buffer[offset + i], i += d, nBits -= 8) { }

            m = e & ((1 << (-nBits)) - 1)
            e >>= (-nBits)
            nBits += mLen
            for (; nBits > 0; m = (m * 256) + buffer[offset + i], i += d, nBits -= 8) { }

            if (e === 0) {
                e = 1 - eBias
            } else if (e === eMax) {
                return m ? NaN : ((s ? -1 : 1) * Infinity)
            } else {
                m = m + Math.pow(2, mLen)
                e = e - eBias
            }
            return (s ? -1 : 1) * m * Math.pow(2, e - mLen)
        }

        exports.write = function (buffer, value, offset, isLE, mLen, nBytes) {
            var e, m, c
            var eLen = (nBytes * 8) - mLen - 1
            var eMax = (1 << eLen) - 1
            var eBias = eMax >> 1
            var rt = (mLen === 23 ? Math.pow(2, -24) - Math.pow(2, -77) : 0)
            var i = isLE ? 0 : (nBytes - 1)
            var d = isLE ? 1 : -1
            var s = value < 0 || (value === 0 && 1 / value < 0) ? 1 : 0

            value = Math.abs(value)

            if (isNaN(value) || value === Infinity) {
                m = isNaN(value) ? 1 : 0
                e = eMax
            } else {
                e = Math.floor(Math.log(value) / Math.LN2)
                if (value * (c = Math.pow(2, -e)) < 1) {
                    e--
                    c *= 2
                }
                if (e + eBias >= 1) {
                    value += rt / c
                } else {
                    value += rt * Math.pow(2, 1 - eBias)
                }
                if (value * c >= 2) {
                    e++
                    c /= 2
                }

                if (e + eBias >= eMax) {
                    m = 0
                    e = eMax
                } else if (e + eBias >= 1) {
                    m = ((value * c) - 1) * Math.pow(2, mLen)
                    e = e + eBias
                } else {
                    m = value * Math.pow(2, eBias - 1) * Math.pow(2, mLen)
                    e = 0
                }
            }

            for (; mLen >= 8; buffer[offset + i] = m & 0xff, i += d, m /= 256, mLen -= 8) { }

            e = (e << mLen) | m
            eLen += mLen
            for (; eLen > 0; buffer[offset + i] = e & 0xff, i += d, e /= 256, eLen -= 8) { }

            buffer[offset + i - d] |= s * 128
        }

    }, {}],
    8: [function (require, module, exports) {
        (function (process) {
            (function () {
                // 'path' module extracted from Node.js v8.11.1 (only the posix part)
                // transplited with Babel

                // Copyright Joyent, Inc. and other Node contributors.
                //
                // Permission is hereby granted, free of charge, to any person obtaining a
                // copy of this software and associated documentation files (the
                // "Software"), to deal in the Software without restriction, including
                // without limitation the rights to use, copy, modify, merge, publish,
                // distribute, sublicense, and/or sell copies of the Software, and to permit
                // persons to whom the Software is furnished to do so, subject to the
                // following conditions:
                //
                // The above copyright notice and this permission notice shall be included
                // in all copies or substantial portions of the Software.
                //
                // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
                // OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
                // MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
                // NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
                // DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                // OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
                // USE OR OTHER DEALINGS IN THE SOFTWARE.

                'use strict';

                function assertPath(path) {
                    if (typeof path !== 'string') {
                        throw new TypeError('Path must be a string. Received ' + JSON.stringify(path));
                    }
                }

                // Resolves . and .. elements in a path with directory names
                function normalizeStringPosix(path, allowAboveRoot) {
                    var res = '';
                    var lastSegmentLength = 0;
                    var lastSlash = -1;
                    var dots = 0;
                    var code;
                    for (var i = 0; i <= path.length; ++i) {
                        if (i < path.length)
                            code = path.charCodeAt(i);
                        else if (code === 47 /*/*/)
                            break;
                        else
                            code = 47 /*/*/;
                        if (code === 47 /*/*/) {
                            if (lastSlash === i - 1 || dots === 1) {
                                // NOOP
                            } else if (lastSlash !== i - 1 && dots === 2) {
                                if (res.length < 2 || lastSegmentLength !== 2 || res.charCodeAt(res.length - 1) !== 46 /*.*/ || res.charCodeAt(res.length - 2) !== 46 /*.*/) {
                                    if (res.length > 2) {
                                        var lastSlashIndex = res.lastIndexOf('/');
                                        if (lastSlashIndex !== res.length - 1) {
                                            if (lastSlashIndex === -1) {
                                                res = '';
                                                lastSegmentLength = 0;
                                            } else {
                                                res = res.slice(0, lastSlashIndex);
                                                lastSegmentLength = res.length - 1 - res.lastIndexOf('/');
                                            }
                                            lastSlash = i;
                                            dots = 0;
                                            continue;
                                        }
                                    } else if (res.length === 2 || res.length === 1) {
                                        res = '';
                                        lastSegmentLength = 0;
                                        lastSlash = i;
                                        dots = 0;
                                        continue;
                                    }
                                }
                                if (allowAboveRoot) {
                                    if (res.length > 0)
                                        res += '/..';
                                    else
                                        res = '..';
                                    lastSegmentLength = 2;
                                }
                            } else {
                                if (res.length > 0)
                                    res += '/' + path.slice(lastSlash + 1, i);
                                else
                                    res = path.slice(lastSlash + 1, i);
                                lastSegmentLength = i - lastSlash - 1;
                            }
                            lastSlash = i;
                            dots = 0;
                        } else if (code === 46 /*.*/ && dots !== -1) {
                            ++dots;
                        } else {
                            dots = -1;
                        }
                    }
                    return res;
                }

                function _format(sep, pathObject) {
                    var dir = pathObject.dir || pathObject.root;
                    var base = pathObject.base || (pathObject.name || '') + (pathObject.ext || '');
                    if (!dir) {
                        return base;
                    }
                    if (dir === pathObject.root) {
                        return dir + base;
                    }
                    return dir + sep + base;
                }

                var posix = {
                    // path.resolve([from ...], to)
                    resolve: function resolve() {
                        var resolvedPath = '';
                        var resolvedAbsolute = false;
                        var cwd;

                        for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
                            var path;
                            if (i >= 0)
                                path = arguments[i];
                            else {
                                if (cwd === undefined)
                                    cwd = process.cwd();
                                path = cwd;
                            }

                            assertPath(path);

                            // Skip empty entries
                            if (path.length === 0) {
                                continue;
                            }

                            resolvedPath = path + '/' + resolvedPath;
                            resolvedAbsolute = path.charCodeAt(0) === 47 /*/*/;
                        }

                        // At this point the path should be resolved to a full absolute path, but
                        // handle relative paths to be safe (might happen when process.cwd() fails)

                        // Normalize the path
                        resolvedPath = normalizeStringPosix(resolvedPath, !resolvedAbsolute);

                        if (resolvedAbsolute) {
                            if (resolvedPath.length > 0)
                                return '/' + resolvedPath;
                            else
                                return '/';
                        } else if (resolvedPath.length > 0) {
                            return resolvedPath;
                        } else {
                            return '.';
                        }
                    },

                    normalize: function normalize(path) {
                        assertPath(path);

                        if (path.length === 0) return '.';

                        var isAbsolute = path.charCodeAt(0) === 47 /*/*/;
                        var trailingSeparator = path.charCodeAt(path.length - 1) === 47 /*/*/;

                        // Normalize the path
                        path = normalizeStringPosix(path, !isAbsolute);

                        if (path.length === 0 && !isAbsolute) path = '.';
                        if (path.length > 0 && trailingSeparator) path += '/';

                        if (isAbsolute) return '/' + path;
                        return path;
                    },

                    isAbsolute: function isAbsolute(path) {
                        assertPath(path);
                        return path.length > 0 && path.charCodeAt(0) === 47 /*/*/;
                    },

                    join: function join() {
                        if (arguments.length === 0)
                            return '.';
                        var joined;
                        for (var i = 0; i < arguments.length; ++i) {
                            var arg = arguments[i];
                            assertPath(arg);
                            if (arg.length > 0) {
                                if (joined === undefined)
                                    joined = arg;
                                else
                                    joined += '/' + arg;
                            }
                        }
                        if (joined === undefined)
                            return '.';
                        return posix.normalize(joined);
                    },

                    relative: function relative(from, to) {
                        assertPath(from);
                        assertPath(to);

                        if (from === to) return '';

                        from = posix.resolve(from);
                        to = posix.resolve(to);

                        if (from === to) return '';

                        // Trim any leading backslashes
                        var fromStart = 1;
                        for (; fromStart < from.length; ++fromStart) {
                            if (from.charCodeAt(fromStart) !== 47 /*/*/)
                                break;
                        }
                        var fromEnd = from.length;
                        var fromLen = fromEnd - fromStart;

                        // Trim any leading backslashes
                        var toStart = 1;
                        for (; toStart < to.length; ++toStart) {
                            if (to.charCodeAt(toStart) !== 47 /*/*/)
                                break;
                        }
                        var toEnd = to.length;
                        var toLen = toEnd - toStart;

                        // Compare paths to find the longest common path from root
                        var length = fromLen < toLen ? fromLen : toLen;
                        var lastCommonSep = -1;
                        var i = 0;
                        for (; i <= length; ++i) {
                            if (i === length) {
                                if (toLen > length) {
                                    if (to.charCodeAt(toStart + i) === 47 /*/*/) {
                                        // We get here if `from` is the exact base path for `to`.
                                        // For example: from='/foo/bar'; to='/foo/bar/baz'
                                        return to.slice(toStart + i + 1);
                                    } else if (i === 0) {
                                        // We get here if `from` is the root
                                        // For example: from='/'; to='/foo'
                                        return to.slice(toStart + i);
                                    }
                                } else if (fromLen > length) {
                                    if (from.charCodeAt(fromStart + i) === 47 /*/*/) {
                                        // We get here if `to` is the exact base path for `from`.
                                        // For example: from='/foo/bar/baz'; to='/foo/bar'
                                        lastCommonSep = i;
                                    } else if (i === 0) {
                                        // We get here if `to` is the root.
                                        // For example: from='/foo'; to='/'
                                        lastCommonSep = 0;
                                    }
                                }
                                break;
                            }
                            var fromCode = from.charCodeAt(fromStart + i);
                            var toCode = to.charCodeAt(toStart + i);
                            if (fromCode !== toCode)
                                break;
                            else if (fromCode === 47 /*/*/)
                                lastCommonSep = i;
                        }

                        var out = '';
                        // Generate the relative path based on the path difference between `to`
                        // and `from`
                        for (i = fromStart + lastCommonSep + 1; i <= fromEnd; ++i) {
                            if (i === fromEnd || from.charCodeAt(i) === 47 /*/*/) {
                                if (out.length === 0)
                                    out += '..';
                                else
                                    out += '/..';
                            }
                        }

                        // Lastly, append the rest of the destination (`to`) path that comes after
                        // the common path parts
                        if (out.length > 0)
                            return out + to.slice(toStart + lastCommonSep);
                        else {
                            toStart += lastCommonSep;
                            if (to.charCodeAt(toStart) === 47 /*/*/)
                                ++toStart;
                            return to.slice(toStart);
                        }
                    },

                    _makeLong: function _makeLong(path) {
                        return path;
                    },

                    dirname: function dirname(path) {
                        assertPath(path);
                        if (path.length === 0) return '.';
                        var code = path.charCodeAt(0);
                        var hasRoot = code === 47 /*/*/;
                        var end = -1;
                        var matchedSlash = true;
                        for (var i = path.length - 1; i >= 1; --i) {
                            code = path.charCodeAt(i);
                            if (code === 47 /*/*/) {
                                if (!matchedSlash) {
                                    end = i;
                                    break;
                                }
                            } else {
                                // We saw the first non-path separator
                                matchedSlash = false;
                            }
                        }

                        if (end === -1) return hasRoot ? '/' : '.';
                        if (hasRoot && end === 1) return '//';
                        return path.slice(0, end);
                    },

                    basename: function basename(path, ext) {
                        if (ext !== undefined && typeof ext !== 'string') throw new TypeError('"ext" argument must be a string');
                        assertPath(path);

                        var start = 0;
                        var end = -1;
                        var matchedSlash = true;
                        var i;

                        if (ext !== undefined && ext.length > 0 && ext.length <= path.length) {
                            if (ext.length === path.length && ext === path) return '';
                            var extIdx = ext.length - 1;
                            var firstNonSlashEnd = -1;
                            for (i = path.length - 1; i >= 0; --i) {
                                var code = path.charCodeAt(i);
                                if (code === 47 /*/*/) {
                                    // If we reached a path separator that was not part of a set of path
                                    // separators at the end of the string, stop now
                                    if (!matchedSlash) {
                                        start = i + 1;
                                        break;
                                    }
                                } else {
                                    if (firstNonSlashEnd === -1) {
                                        // We saw the first non-path separator, remember this index in case
                                        // we need it if the extension ends up not matching
                                        matchedSlash = false;
                                        firstNonSlashEnd = i + 1;
                                    }
                                    if (extIdx >= 0) {
                                        // Try to match the explicit extension
                                        if (code === ext.charCodeAt(extIdx)) {
                                            if (--extIdx === -1) {
                                                // We matched the extension, so mark this as the end of our path
                                                // component
                                                end = i;
                                            }
                                        } else {
                                            // Extension does not match, so our result is the entire path
                                            // component
                                            extIdx = -1;
                                            end = firstNonSlashEnd;
                                        }
                                    }
                                }
                            }

                            if (start === end) end = firstNonSlashEnd;
                            else if (end === -1) end = path.length;
                            return path.slice(start, end);
                        } else {
                            for (i = path.length - 1; i >= 0; --i) {
                                if (path.charCodeAt(i) === 47 /*/*/) {
                                    // If we reached a path separator that was not part of a set of path
                                    // separators at the end of the string, stop now
                                    if (!matchedSlash) {
                                        start = i + 1;
                                        break;
                                    }
                                } else if (end === -1) {
                                    // We saw the first non-path separator, mark this as the end of our
                                    // path component
                                    matchedSlash = false;
                                    end = i + 1;
                                }
                            }

                            if (end === -1) return '';
                            return path.slice(start, end);
                        }
                    },

                    extname: function extname(path) {
                        assertPath(path);
                        var startDot = -1;
                        var startPart = 0;
                        var end = -1;
                        var matchedSlash = true;
                        // Track the state of characters (if any) we see before our first dot and
                        // after any path separator we find
                        var preDotState = 0;
                        for (var i = path.length - 1; i >= 0; --i) {
                            var code = path.charCodeAt(i);
                            if (code === 47 /*/*/) {
                                // If we reached a path separator that was not part of a set of path
                                // separators at the end of the string, stop now
                                if (!matchedSlash) {
                                    startPart = i + 1;
                                    break;
                                }
                                continue;
                            }
                            if (end === -1) {
                                // We saw the first non-path separator, mark this as the end of our
                                // extension
                                matchedSlash = false;
                                end = i + 1;
                            }
                            if (code === 46 /*.*/) {
                                // If this is our first dot, mark it as the start of our extension
                                if (startDot === -1)
                                    startDot = i;
                                else if (preDotState !== 1)
                                    preDotState = 1;
                            } else if (startDot !== -1) {
                                // We saw a non-dot and non-path separator before our dot, so we should
                                // have a good chance at having a non-empty extension
                                preDotState = -1;
                            }
                        }

                        if (startDot === -1 || end === -1 ||
                            // We saw a non-dot character immediately before the dot
                            preDotState === 0 ||
                            // The (right-most) trimmed path component is exactly '..'
                            preDotState === 1 && startDot === end - 1 && startDot === startPart + 1) {
                            return '';
                        }
                        return path.slice(startDot, end);
                    },

                    format: function format(pathObject) {
                        if (pathObject === null || typeof pathObject !== 'object') {
                            throw new TypeError('The "pathObject" argument must be of type Object. Received type ' + typeof pathObject);
                        }
                        return _format('/', pathObject);
                    },

                    parse: function parse(path) {
                        assertPath(path);

                        var ret = {
                            root: '',
                            dir: '',
                            base: '',
                            ext: '',
                            name: ''
                        };
                        if (path.length === 0) return ret;
                        var code = path.charCodeAt(0);
                        var isAbsolute = code === 47 /*/*/;
                        var start;
                        if (isAbsolute) {
                            ret.root = '/';
                            start = 1;
                        } else {
                            start = 0;
                        }
                        var startDot = -1;
                        var startPart = 0;
                        var end = -1;
                        var matchedSlash = true;
                        var i = path.length - 1;

                        // Track the state of characters (if any) we see before our first dot and
                        // after any path separator we find
                        var preDotState = 0;

                        // Get non-dir info
                        for (; i >= start; --i) {
                            code = path.charCodeAt(i);
                            if (code === 47 /*/*/) {
                                // If we reached a path separator that was not part of a set of path
                                // separators at the end of the string, stop now
                                if (!matchedSlash) {
                                    startPart = i + 1;
                                    break;
                                }
                                continue;
                            }
                            if (end === -1) {
                                // We saw the first non-path separator, mark this as the end of our
                                // extension
                                matchedSlash = false;
                                end = i + 1;
                            }
                            if (code === 46 /*.*/) {
                                // If this is our first dot, mark it as the start of our extension
                                if (startDot === -1) startDot = i;
                                else if (preDotState !== 1) preDotState = 1;
                            } else if (startDot !== -1) {
                                // We saw a non-dot and non-path separator before our dot, so we should
                                // have a good chance at having a non-empty extension
                                preDotState = -1;
                            }
                        }

                        if (startDot === -1 || end === -1 ||
                            // We saw a non-dot character immediately before the dot
                            preDotState === 0 ||
                            // The (right-most) trimmed path component is exactly '..'
                            preDotState === 1 && startDot === end - 1 && startDot === startPart + 1) {
                            if (end !== -1) {
                                if (startPart === 0 && isAbsolute) ret.base = ret.name = path.slice(1, end);
                                else ret.base = ret.name = path.slice(startPart, end);
                            }
                        } else {
                            if (startPart === 0 && isAbsolute) {
                                ret.name = path.slice(1, startDot);
                                ret.base = path.slice(1, end);
                            } else {
                                ret.name = path.slice(startPart, startDot);
                                ret.base = path.slice(startPart, end);
                            }
                            ret.ext = path.slice(startDot, end);
                        }

                        if (startPart > 0) ret.dir = path.slice(0, startPart - 1);
                        else if (isAbsolute) ret.dir = '/';

                        return ret;
                    },

                    sep: '/',
                    delimiter: ':',
                    win32: null,
                    posix: null
                };

                posix.posix = posix;

                module.exports = posix;

            }).call(this)
        }).call(this, require('_process'))
    }, {
        "_process": 9
    }],
    9: [function (require, module, exports) {
        // shim for using process in browser
        var process = module.exports = {};

        // cached from whatever global is present so that test runners that stub it
        // don't break things.  But we need to wrap it in a try catch in case it is
        // wrapped in strict mode code which doesn't define any globals.  It's inside a
        // function because try/catches deoptimize in certain engines.

        var cachedSetTimeout;
        var cachedClearTimeout;

        function defaultSetTimout() {
            throw new Error('setTimeout has not been defined');
        }

        function defaultClearTimeout() {
            throw new Error('clearTimeout has not been defined');
        }
        (function () {
            try {
                if (typeof setTimeout === 'function') {
                    cachedSetTimeout = setTimeout;
                } else {
                    cachedSetTimeout = defaultSetTimout;
                }
            } catch (e) {
                cachedSetTimeout = defaultSetTimout;
            }
            try {
                if (typeof clearTimeout === 'function') {
                    cachedClearTimeout = clearTimeout;
                } else {
                    cachedClearTimeout = defaultClearTimeout;
                }
            } catch (e) {
                cachedClearTimeout = defaultClearTimeout;
            }
        }())

        function runTimeout(fun) {
            if (cachedSetTimeout === setTimeout) {
                //normal enviroments in sane situations
                return setTimeout(fun, 0);
            }
            // if setTimeout wasn't available but was latter defined
            if ((cachedSetTimeout === defaultSetTimout || !cachedSetTimeout) && setTimeout) {
                cachedSetTimeout = setTimeout;
                return setTimeout(fun, 0);
            }
            try {
                // when when somebody has screwed with setTimeout but no I.E. maddness
                return cachedSetTimeout(fun, 0);
            } catch (e) {
                try {
                    // When we are in I.E. but the script has been evaled so I.E. doesn't trust the global object when called normally
                    return cachedSetTimeout.call(null, fun, 0);
                } catch (e) {
                    // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error
                    return cachedSetTimeout.call(this, fun, 0);
                }
            }


        }

        function runClearTimeout(marker) {
            if (cachedClearTimeout === clearTimeout) {
                //normal enviroments in sane situations
                return clearTimeout(marker);
            }
            // if clearTimeout wasn't available but was latter defined
            if ((cachedClearTimeout === defaultClearTimeout || !cachedClearTimeout) && clearTimeout) {
                cachedClearTimeout = clearTimeout;
                return clearTimeout(marker);
            }
            try {
                // when when somebody has screwed with setTimeout but no I.E. maddness
                return cachedClearTimeout(marker);
            } catch (e) {
                try {
                    // When we are in I.E. but the script has been evaled so I.E. doesn't  trust the global object when called normally
                    return cachedClearTimeout.call(null, marker);
                } catch (e) {
                    // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error.
                    // Some versions of I.E. have different rules for clearTimeout vs setTimeout
                    return cachedClearTimeout.call(this, marker);
                }
            }



        }
        var queue = [];
        var draining = false;
        var currentQueue;
        var queueIndex = -1;

        function cleanUpNextTick() {
            if (!draining || !currentQueue) {
                return;
            }
            draining = false;
            if (currentQueue.length) {
                queue = currentQueue.concat(queue);
            } else {
                queueIndex = -1;
            }
            if (queue.length) {
                drainQueue();
            }
        }

        function drainQueue() {
            if (draining) {
                return;
            }
            var timeout = runTimeout(cleanUpNextTick);
            draining = true;

            var len = queue.length;
            while (len) {
                currentQueue = queue;
                queue = [];
                while (++queueIndex < len) {
                    if (currentQueue) {
                        currentQueue[queueIndex].run();
                    }
                }
                queueIndex = -1;
                len = queue.length;
            }
            currentQueue = null;
            draining = false;
            runClearTimeout(timeout);
        }

        process.nextTick = function (fun) {
            var args = new Array(arguments.length - 1);
            if (arguments.length > 1) {
                for (var i = 1; i < arguments.length; i++) {
                    args[i - 1] = arguments[i];
                }
            }
            queue.push(new Item(fun, args));
            if (queue.length === 1 && !draining) {
                runTimeout(drainQueue);
            }
        };

        // v8 likes predictible objects
        function Item(fun, array) {
            this.fun = fun;
            this.array = array;
        }
        Item.prototype.run = function () {
            this.fun.apply(null, this.array);
        };
        process.title = 'browser';
        process.browser = true;
        process.env = {};
        process.argv = [];
        process.version = ''; // empty string to avoid regexp issues
        process.versions = {};

        function noop() { }

        process.on = noop;
        process.addListener = noop;
        process.once = noop;
        process.off = noop;
        process.removeListener = noop;
        process.removeAllListeners = noop;
        process.emit = noop;
        process.prependListener = noop;
        process.prependOnceListener = noop;

        process.listeners = function (name) {
            return []
        }

        process.binding = function (name) {
            throw new Error('process.binding is not supported');
        };

        process.cwd = function () {
            return '/'
        };
        process.chdir = function (dir) {
            throw new Error('process.chdir is not supported');
        };
        process.umask = function () {
            return 0;
        };

    }, {}]
}, {}, [1]);