async function doRun() {

function log(str) { }
globalObject.log = log

globalObject.getInput = function() {
    return "t"
}

let verifyVideo = false
let videoSum = 0
globalObject.updateVideo = function(vec) {
    if (vec.length != 4 * 256 * 240)
        throw new Error("Wrong vec length")
    if (!verifyVideo)
        return
    for (let i = 0, length = globalObject.video.length; i < length; ++i) {
        videoSum += vec[i]
    }
}

let ab = new ArrayBuffer(Int16Array.BYTES_PER_ELEMENT * 4 * 256 * 240)
globalObject.video = new Int16Array(ab)

let start = benchmarkTime()
if (!Module.wasmBinary.length || !Module.romBinary.length)
    throw new Error("Needed binary not loaded");

await init(Module.wasmBinary)
loadRom(Module.romBinary)
js_tick()
reportCompileTime(benchmarkTime() - start)
start = benchmarkTime()

let frames = 40 * 60
for (let i = 0; i < frames; ++i) {
    js_tick()
}

verifyVideo = true
js_tick()

if (video.length != 256 * 240 * 4)
    throw "Length is wrong"

if (videoSum != 45833688)
    throw "Wrong video sum, the picture is wrong: " + videoSum

reportRunTime(benchmarkTime() - start)
}