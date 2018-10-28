var TRACK_KIND = {
    AUDIO: 0,
    VIDEO: 1,
    TEXT: 2,
};

function stringToArray(string) {
    return string.split("").map(function(c){ return c.charCodeAt(0); });
}

var SAMPLE_FLAG = {
    NONE: 0,
    SYNC: 1 << 0,
    CORRUPTED: 1 << 1,
    DROPPED: 1 << 2,
    DELAYED: 1 << 3,
};

function makeASample(presentationTime, decodeTime, duration, trackID, flags, generation, timeScale) {
    if (typeof timeScale === 'undefined')
        timeScale = 1000
    var byteLength = 30;
    var buffer = new ArrayBuffer(byteLength);
    var array = new Uint8Array(buffer);
    array.set(stringToArray('smpl'));

    var view = new DataView(buffer);
    view.setUint32(4, byteLength, true);
    view.setInt32(8, timeScale, true);
    view.setInt32(12, presentationTime * timeScale, true);
    view.setInt32(16, decodeTime * timeScale, true);
    view.setInt32(20, duration * timeScale, true);
    view.setInt32(24, trackID, true);
    view.setUint8(28, flags);
    view.setUint8(29, generation);

    return buffer
}

function concatenateSamples(samples) {
    var byteLength = 0;
    samples.forEach(function(sample) { byteLength += sample.byteLength; });
    var buffer = new ArrayBuffer(byteLength);

    var offset = 0;
    samples.forEach(function(sample){
        var sourceArray = new Uint8Array(sample);
        var destArray = new Uint8Array(buffer, offset, sourceArray.byteLength);
        destArray.set(sourceArray);
        offset += sourceArray.byteLength;
    });

    return buffer;
}

function makeATrack(trackID, codec, flags) {
    var byteLength = 17;
    var buffer = new ArrayBuffer(byteLength);
    var array = new Uint8Array(buffer);
    array.set(stringToArray('trak'));

    var view = new DataView(buffer);
    view.setUint32(4, byteLength, true);
    view.setInt32(8, trackID, true);

    var codecArray = new Uint8Array(buffer, 12, 4);
    codecArray.set(stringToArray(codec));

    view.setUint8(16, flags, true);

    return buffer;
}

function makeAInit(duration, tracks) {
    var byteLength = 16 + (17 * tracks.length);
    var buffer = new ArrayBuffer(byteLength);
    var array = new Uint8Array(buffer);
    array.set(stringToArray('init'));

    var view = new DataView(buffer);
    var timeScale = 1000;
    view.setUint32(4, byteLength, true);
    view.setInt32(8, duration * timeScale, true);
    view.setInt32(12, timeScale, true);

    var offset = 16;
    tracks.forEach(function(track){
        var sourceArray = new Uint8Array(track);
        var destArray = new Uint8Array(buffer, offset, sourceArray.byteLength);
        destArray.set(sourceArray);
        offset += sourceArray.byteLength;
    });

    return buffer;
}

function makeAnInvalidBox() {
    var byteLength = 12;
    var buffer = new ArrayBuffer(byteLength);
    var array = new Uint8Array(buffer);
    array.set(stringToArray('invl'));

    var view = new DataView(buffer);
    view.setUint32(8, 0xFFFF, true);

    return buffer;
}

