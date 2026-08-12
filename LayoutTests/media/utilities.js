function once(target, name, callback) {
    let promise = new Promise((resolve, reject) => {
        target.addEventListener(name, (event) => {
            resolve(event);
        }, { once: true });
    });
    if (callback)
        promise.then(callback);
    return promise;
}

function fetchWithXHR(uri, onLoadFunction) {
    let p = new Promise((resolve, reject) => {
        let xhr = new XMLHttpRequest();
        xhr.open("GET", uri, true);
        xhr.responseType = "arraybuffer";
        xhr.addEventListener("load", () => {
            resolve(xhr.response);
        });
        xhr.send();
    });

    if (onLoadFunction)
        p.then(onLoadFunction);

    return p;
};

function loadSegment(sb, typedArrayOrArrayBuffer) {
    let typedArray = (typedArrayOrArrayBuffer instanceof ArrayBuffer)
        ? new Uint8Array(typedArrayOrArrayBuffer)
        : typedArrayOrArrayBuffer;
    return new Promise((resolve, reject) => {
        once(sb, 'update').then(() => { resolve(); });
        sb.appendBuffer(typedArray);
    });
}

function fetchAndLoad(sb, prefix, chunks, suffix) {
    // Fetch the buffers in parallel.
    let buffers = {};
    let fetches = [];
    for (var chunk of chunks)
        fetches.push(fetchWithXHR(prefix + chunk + suffix).then(((c, x) => buffers[c] = x).bind(null, chunk)));

    // Load them in series, as required per spec.
    return Promise.all(fetches).then(() => {
        let rv = Promise.resolve();
        for (let chunk of chunks)
            rv = rv.then(loadSegment.bind(null, sb, buffers[chunk]));
        return rv;
    });
}

const delay = ms => new Promise(res => setTimeout(res, ms));

function waitForVideoFrame(video, cb) {
    const p = new Promise((resolve) => {
        video.requestVideoFrameCallback((now, metadata) => resolve([now, metadata]));
    });

    if (cb)
        p.then(cb);
    return p;
}

function waitForVideoFrameWithTimeout(video, time, message) {
    const framePromise = new Promise((resolve) => {
        video.requestVideoFrameCallback((now, metadata) => resolve([now, metadata]));
    });
    const timeoutPromise = new Promise((resolve) => {
        setTimeout(resolve, time, 'timeout');
    });

    return Promise.race([
        framePromise, 
        timeoutPromise,
    ]).then(result => {
        if (result === 'timeout')
            return Promise.reject(new Error(message));

        return Promise.resolve(result);
    });
}

function waitForVideoFrameUntil(video, time, cb) {
    const p = new Promise(resolve => {
        const callback = ((now, metadata) => {
            if (metadata.mediaTime >= time) {
                resolve([now, metadata]);
                return;
            }
            video.requestVideoFrameCallback(callback);
        });
        video.requestVideoFrameCallback(callback);
    });
    if (cb)
        p.then(cb);
    return p;
}

function timeRangesToString(timeRanges) {
    if (!timeRanges?.length)
        return "[]";

    const ranges = [];
    for (let i = 0; i < timeRanges?.length; i++) {
      const range = "[" + [timeRanges.start(i)  + ", " + timeRanges.end(i)] + ")";
      ranges.push(range);
    }
    return ranges.toString();
}

// Play a media element inside a user gesture, ignoring the AbortError that play() rejects with when
// a still-pending play() is interrupted by a later pause() or by teardown (spec behavior, not a
// failure). Any other rejection is rethrown so a genuine play() failure still surfaces. Requires the
// Internals API.
function playIgnoringAbort(mediaElement) {
    internals.withUserGesture(() => {
        mediaElement.play().catch(error => {
            if (error.name !== "AbortError")
                throw error;
        });
    });
}

// Create a near-silent AudioContext under a user gesture and wait until it is actually running before
// returning it, so a caller polling internals.audioSessionActive() is timing the audio-session
// activation and not WebAudio context startup. The tone renders (activating the audio session) but is
// routed through a 0.001 gain so it is inaudible at a desk. Requires the Internals API. The caller owns
// the returned context and should close() it when done.
async function startNearSilentAudioContext() {
    let context;
    let resumePromise;
    internals.withUserGesture(() => {
        context = new AudioContext();
        const oscillator = context.createOscillator();
        const gain = context.createGain();
        gain.gain.value = 0.001;
        oscillator.connect(gain);
        gain.connect(context.destination);
        oscillator.start();
        resumePromise = context.resume();
    });
    await resumePromise;
    return context;
}

function waitForAudioSessionActiveState(active) {
    return new Promise((resolve, reject) => {
        if (!window.internals) {
            reject(new Error("waitForAudioSessionActiveState requires window.internals"));
            return;
        }
        let tries = 0;
        const check = () => {
            if (internals.audioSessionActive() === active)
                resolve();
            else if (++tries >= 200)
                reject(new Error(`audio session did not reach active=${active}`));
            else
                setTimeout(check, 10);
        };
        check();
    });
}
