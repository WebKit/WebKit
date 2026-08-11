const WORKER_COUNT = 10;

function broadcastChannelTest(createValueFn, expected) {
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    var output = document.getElementById("output");
    function log(msg) {
        output.textContent += msg + "\n";
    }

    function done() {
        clearTimeout(timer);
        workers.forEach(function(w) { w.terminate(); });
        if (window.testRunner)
            testRunner.notifyDone();
    }

    var workerCode = [
        'function inspect(d) {',
        '    if (d === null) return "null";',
        '    if (d === undefined) return "undefined";',
        '    if (typeof d === "string") return "string:" + d;',
        '    if (typeof d === "number") return "number:" + d;',
        '    if (typeof d === "boolean") return "boolean:" + d;',
        '    if (d instanceof Date) return "Date:" + d.getTime();',
        '    if (d instanceof RegExp) return "RegExp:" + d.toString();',
        '    if (d instanceof DOMException) return "DOMException:" + d.name + ":" + d.message;',
        '    if (d instanceof Error) return d.constructor.name + ":" + d.message;',
        '    if (typeof SharedArrayBuffer !== "undefined" && d instanceof SharedArrayBuffer) return "SharedArrayBuffer:" + d.byteLength;',
        '    if (d instanceof ArrayBuffer) return "ArrayBuffer:" + d.byteLength;',
        '    if (d instanceof DataView) return "DataView:" + d.byteLength;',
        '    if (ArrayBuffer.isView(d)) return d.constructor.name + ":" + d.length;',
        '    if (d instanceof Blob) return "Blob:" + d.size + ":" + d.type;',
        '    if (typeof ImageBitmap !== "undefined" && d instanceof ImageBitmap) return "ImageBitmap:" + d.width + "x" + d.height;',
        '    if (d instanceof ImageData) return "ImageData:" + d.width + "x" + d.height;',
        '    if (d instanceof Map) return "Map:" + d.size;',
        '    if (d instanceof Set) return "Set:" + d.size;',
        '    if (Array.isArray(d)) return "Array:" + d.length;',
        '    if (typeof EncodedVideoChunk !== "undefined" && d instanceof EncodedVideoChunk) return "EncodedVideoChunk:" + d.type + ":" + d.byteLength;',
        '    if (typeof VideoFrame !== "undefined" && d instanceof VideoFrame) {',
		'        var result = "VideoFrame:" + d.codedWidth + "x" + d.codedHeight;',
		'        d.close();',
		'        return result;',
		'    }',
        '    if (typeof EncodedAudioChunk !== "undefined" && d instanceof EncodedAudioChunk) return "EncodedAudioChunk:" + d.type + ":" + d.byteLength;',
        '    if (typeof AudioData !== "undefined" && d instanceof AudioData) return "AudioData:" + d.numberOfChannels + ":" + d.numberOfFrames;',
        '    if (typeof MediaStreamTrack !== "undefined" && d instanceof MediaStreamTrack) return "MediaStreamTrack:" + d.kind;',
        '    if (typeof MediaStreamTrackHandle !== "undefined" && d instanceof MediaStreamTrackHandle) return "MediaStreamTrackHandle";',
        '    if (typeof WebAssembly !== "undefined" && d instanceof WebAssembly.Module) return "WebAssembly.Module";',
        '    if (typeof WebAssembly !== "undefined" && d instanceof WebAssembly.Memory) return "WebAssembly.Memory:" + d.buffer.byteLength;',
        '    if (typeof d === "object") return "Object";',
        '    return "unknown:" + typeof d;',
        '}',
        'var bc = new BroadcastChannel("test");',
        'bc.onmessage = function(event) {',
        '    try {',
        '        self.postMessage(inspect(event.data));',
        '    } catch (e) {',
        '        self.postMessage("ERROR:" + e.message);',
        '    }',
        '};',
        'self.postMessage("READY");'
    ].join("\n");

    var blob = new Blob([workerCode], { type: "application/javascript" });
    var url = URL.createObjectURL(blob);
    var readyCount = 0;
    var results = [];
    var workers = [];

    var timer = setTimeout(function() {
        log("FAIL: Test timed out (" + readyCount + " of " + WORKER_COUNT + " workers ready, " + results.length + " of " + WORKER_COUNT + " results received)");
        done();
    }, 10000);

    for (var i = 0; i < WORKER_COUNT; i++) {
        var w = new Worker(url);
        workers.push(w);
        w.onmessage = function(e) {
            if (e.data === "READY") {
                if (++readyCount === WORKER_COUNT)
                    postValue();
            } else {
                results.push(e.data);
                if (results.length === WORKER_COUNT)
                    checkResults();
            }
        };
        w.onerror = function(e) {
            log("FAIL: Worker error: " + e.message);
            done();
        };
    }

    function postValue() {
        try {
            Promise.resolve(createValueFn()).then(function(value) {
                var bc = new BroadcastChannel("test");
                bc.postMessage(value);
                bc.close();
            }, function(e) {
                log("FAIL: Error creating value: " + e.message);
                done();
            });
        } catch (e) {
            log("FAIL: Error creating value: " + e.message);
            done();
        }
    }

    function checkResults() {
        URL.revokeObjectURL(url);
        var allSame = results.every(function(r) { return r === results[0]; });
        if (allSame && results[0] === expected)
            log("PASS: All " + WORKER_COUNT + " workers received \"" + expected + "\"");
        else if (!allSame)
            log("FAIL: Workers received mismatched results: " + JSON.stringify(results));
        else
            log("FAIL: Expected \"" + expected + "\" but got \"" + results[0] + "\"");
        done();
    }
}
