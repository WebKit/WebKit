// Upload path tests for webgl/video-formats: which path (hardware or software) a video or VideoFrame
// upload takes and what it stores, through the test hooks internals.setWebGLVideoUploadPath and
// internals.webglLastVideoUpload.
//
// Needs js-test.js, video-formats-manifest.js, video-formats-test-utils.js and webgl-video-sink.js.
//
// Every upload goes into a new texture whose target level (every face of a cube map, layer 0 of a 2D
// array) is defined first with a fill colour, so that an upload that must leave the texture unchanged
// (a spec error, or a forced hardware upload that is not possible) can be checked. A level that is
// uploaded into is checked against the shared oracle: the frame's own R'G'B' values (no colour
// management), mapped to the format's channels and bit depth. sRGB formats store those values as they
// are, as a texImage2D of the same pixels would.
//
// Only frame 0 of each video is used; the videos are not played or seeked beyond loading.

"use strict";

var UploadPathTest = (() => {
    const VF = VideoFormats;
    const Sink = WebGLVideoSink;

    const NO_ERROR = 0;
    const INVALID_VALUE = 0x0501;
    const INVALID_OPERATION = 0x0502;

    const fillColor = [0, 128, 0, 64];
    const fillSize = 4;
    const defaultTriple = { internalformat: "RGBA", format: "RGBA", type: "UNSIGNED_BYTE" };

    // ---- Test set-up ----

    function run(fn)
    {
        window.jsTestIsAsync = true;
        Sink.disableConsoleErrors();
        (async () => {
            if (!Sink.hasPathHooks()) {
                testFailed("needs internals.setWebGLVideoUploadPath and internals.webglLastVideoUpload");
                return;
            }
            await fn();
        })().catch(e => {
            testFailed(`threw ${e.name}: ${e.message}`);
        }).finally(() => {
            Sink.releaseContexts();
            debug("");
            finishJSTest();
        });
    }

    // Runs fn(gl) with a new context, then releases the context: canvas out of the DOM, references
    // dropped, garbage collection.
    function withContext(version, fn)
    {
        let gl = Sink.createContext(version);
        try {
            fn(gl);
        } finally {
            Sink.releaseContext(gl);
            gl = null;
            Sink.collectGarbage();
        }
    }

    // ---- Sources ----

    function rangeLabel(info)
    {
        return info.range === "pc" ? "fullrange" : info.range === "tv" ? "videorange" : "range untagged";
    }

    function videoDescription(name)
    {
        const info = VF.videoInfo(name);
        return `${name}: ${info.codec} ${info.pixelFormat} ${info.matrix}/${rangeLabel(info)}/${info.primaries}/${info.transfer}`;
    }

    // A source: {label, element, image (what the oracle needs), close()}.
    function videoSource(video, name)
    {
        const info = VF.videoInfo(name);
        return {
            label: "<video>",
            element: video,
            image: { info, width: info.displayWidth, height: info.displayHeight, transform: info.transform, frame: 0, tolerance: VF.defaultTolerance(info) },
            close() { },
        };
    }

    function videoFrameSource(frame, name, label, transform)
    {
        const info = VF.videoInfo(name);
        return {
            label: `${label} (format ${frame.format})`,
            element: frame,
            image: { info, width: frame.displayWidth, height: frame.displayHeight, transform: transform || info.transform, frame: 0, tolerance: VF.defaultTolerance(info) },
            close() { frame.close(); },
        };
    }

    // VideoFrame(video): the frame the video element shows.
    function frameOfVideoSource(video, name)
    {
        return videoFrameSource(new VideoFrame(video), name, "VideoFrame(video)");
    }

    // The first VideoDecoder output frame (not rotated: the decoder is not given the track's display
    // transform).
    function decoderSource(frame, name, acceleration)
    {
        return videoFrameSource(frame, name, `VideoDecoder ${acceleration} frame`, "none");
    }

    // A JS-built VideoFrame(buffer) with the pattern (NV12, RGBA, ...).
    function patternFrameSource(format)
    {
        const { frame, info } = VF.createPatternFrame(format);
        return {
            label: `VideoFrame(${format} buffer)`,
            element: frame,
            image: { info, width: frame.displayWidth, height: frame.displayHeight, transform: "none", frame: 0, tolerance: 3 },
            close() { frame.close(); },
        };
    }

    // ---- Uploads ----

    function isPowerOfTwo(n)
    {
        return n > 0 && !(n & (n - 1));
    }

    // The readback format of a level. EXT_sRGB levels are colour-renderable, so they are read with a
    // framebuffer (the stored bytes) like the WebGL 2 renderable formats.
    function readFormat(gl, internalformat, type)
    {
        const fmt = Sink.formatInfo(gl, internalformat, type);
        if (!Sink.isWebGL2(gl) && internalformat === "SRGB_ALPHA_EXT")
            fmt.read = "fbo";
        return fmt;
    }

    // c: {fn ("texImage2D" (default), "texSubImage2D", "texImage3D"), target (default TEXTURE_2D; a
    // cube face or TEXTURE_2D_ARRAY), level, triple ({internalformat, format, type}, default
    // RGBA/RGBA/UNSIGNED_BYTE), rect (the sized overload with this source sub-rectangle)}.
    function makeCall(c, image)
    {
        const triple = c.triple || defaultTriple;
        const call = { fn: c.fn || "texImage2D", target: c.target || "TEXTURE_2D", level: c.level || 0, internalformat: triple.internalformat, format: triple.format, type: triple.type };
        if (c.rect) {
            call.width = c.rect.w;
            call.height = c.rect.h;
            call.unpack = { skipPixels: c.rect.x, skipRows: c.rect.y };
        }
        if (call.fn === "texImage3D") {
            call.width = call.width || image.width;
            call.height = call.height || image.height;
            call.depth = 1;
        }
        return call;
    }

    function caseLabel(c)
    {
        const triple = c.triple || defaultTriple;
        const parts = [];
        const target = c.target || "TEXTURE_2D";
        if (target !== "TEXTURE_2D")
            parts.push(Sink.shortTarget(target));
        if (c.fn === "texSubImage2D")
            parts.push("at 0,0");
        if (c.level)
            parts.push(`level ${c.level}`);
        if (c.rect)
            parts.push(`source ${c.rect.x},${c.rect.y} ${c.rect.w}x${c.rect.h}`);
        parts.push(Sink.tripleLabel(triple));
        return `${c.fn || "texImage2D"}(${parts.join(", ")})`;
    }

    // The specified outcome: {error, regions, size, fmt}.
    function expectedOutcome(gl, call, image)
    {
        const regions = Sink.uploadRegions(call, image.width, image.height);
        const w = regions[0].dest.w;
        const h = regions[0].dest.h;
        let error = NO_ERROR;
        if (call.fn === "texSubImage2D")
            return { error, regions, size: { w: image.width, h: image.height }, fmt: readFormat(gl, "RGBA", "UNSIGNED_BYTE") };
        if (!Sink.isWebGL2(gl) && call.level > 0 && (!isPowerOfTwo(w) || !isPowerOfTwo(h)))
            error = INVALID_VALUE;
        if (Sink.cubeFaces.includes(call.target) && w !== h)
            error = INVALID_VALUE;
        return { error, regions, size: { w, h }, fmt: readFormat(gl, call.internalformat, call.type) };
    }

    // The destination defined before the upload. The target level is fillSize x fillSize, except for
    // texSubImage2D (the image size) and for a valid cube face upload (every face the upload size, so
    // that the cube map stays cube complete and can be read back). A target level above 0 is part of
    // a complete power-of-two mip chain, so that WebGL 1 can sample it.
    function destinationFor(call, image, expected)
    {
        const bindTarget = Sink.bindTargetOf(call.target);
        let size = { w: fillSize, h: fillSize, d: 1 };
        if (call.fn === "texSubImage2D")
            size = { w: image.width, h: image.height, d: 1 };
        else if (bindTarget === "TEXTURE_CUBE_MAP" && expected.error === NO_ERROR)
            size = { w: expected.size.w, h: expected.size.h, d: 1 };
        const sizes = [];
        sizes[call.level] = size;
        if (call.level) {
            const base = fillSize << call.level;
            for (let level = 0; base >> level >= 1; ++level)
                sizes[level] = { w: base >> level, h: base >> level, d: 1 };
        }
        return Object.assign({ bindTarget, immutable: false, sizes, fill: () => fillColor }, defaultTriple);
    }

    // The largest difference of the checked channels of the patches, and the mismatches. When every
    // translucent patch came out opaque, says so and compares the colours only.
    function comparePixels(readback, patches, tolerances)
    {
        const translucent = patches.filter(p => p.expected && p.expected[3] < 250);
        let failures = [];
        if (translucent.length && translucent.every(p => VF.samplePatch(readback, p.rect)[3] >= 254)) {
            failures.push(`alpha lost: the ${translucent.length} translucent patches are opaque`);
            patches = patches.map(p => p.expected ? Object.assign({ }, p, { expected: p.expected.slice(0, 3).concat([255]) }) : p);
        }
        const mismatches = VF.comparePatches(readback, patches, tolerances);
        if (mismatches.length) {
            let worst = 0;
            for (const p of patches) {
                if (!p.expected)
                    continue;
                const observed = VF.samplePatch(readback, p.rect);
                for (const k of p.checkColor ? [0, 1, 2, 3] : [3])
                    worst = Math.max(worst, Math.abs(observed[k] - p.expected[k]));
            }
            const shown = mismatches.length > 5 ? mismatches.slice(0, 4).concat([`${mismatches.length - 4} more`]) : mismatches;
            failures.push(`up to ${Math.round(worst)} off: ${shown.join("; ")}`);
        }
        return failures;
    }

    // Uploads `source` as `c` says with the upload path `path` ("default", "software" or "hardware")
    // and checks the error, the path and the texture. Returns {record, error, unavailable (the reason
    // when a forced hardware upload was not possible), failures, readback (when the upload stored the
    // image), call, outcome}.
    function runCase(gl, source, c, path = "default")
    {
        const image = source.image;
        const call = makeCall(c, image);
        const outcome = expectedOutcome(gl, call, image);
        const failures = [];
        const texture = gl.createTexture();
        try {
            const setupError = Sink.defineDestination(gl, texture, destinationFor(call, image, outcome));
            if (setupError)
                failures.push(`destination set-up error ${Sink.errorName(setupError)}`);
            Sink.setUploadPath(gl, path);
            let result;
            try {
                result = Sink.upload(gl, texture, call, source.element);
            } finally {
                Sink.setUploadPath(gl, "default");
            }
            const record = result.record;
            if (result.exception)
                failures.push(`threw ${result.exception.name}: ${result.exception.message}`);
            let expectedError = outcome.error;
            let unavailable = null;
            if (path === "hardware" && record && record.path === "none") {
                unavailable = record.reason;
                if (expectedError === NO_ERROR)
                    expectedError = INVALID_OPERATION;
            } else if (path !== "default" && outcome.error === NO_ERROR) {
                if (!record)
                    failures.push(`no upload record, expected the ${path} path`);
                else if (record.path !== path)
                    failures.push(`expected the ${path} path`);
            }
            if (result.error !== expectedError)
                failures.push(`${Sink.errorName(result.error)}, expected ${Sink.errorName(expectedError)}`);
            const uploaded = result.error === NO_ERROR && expectedError === NO_ERROR && !result.exception;
            const level = call.level;
            let readback = null;
            if (uploaded) {
                const fmt = outcome.fmt;
                const size = outcome.size;
                const read = Sink.read(gl, texture, { target: call.target, level, layer: 0, width: size.w, height: size.h, fmt });
                if (read.error)
                    failures.push(`readback error ${Sink.errorName(read.error)}`);
                const observed = Sink.observedSize(read.readback);
                if (observed.w !== size.w || observed.h !== size.h)
                    failures.push(`size ${observed.w}x${observed.h}, expected ${size.w}x${size.h}`);
                const tolerances = Sink.formatTolerance(fmt, image.tolerance);
                for (const region of outcome.regions)
                    failures.push(...comparePixels(read.readback, Sink.regionPatches(image, call, region, fmt), tolerances));
                readback = read.readback;
            } else {
                // Unchanged: the fill colour and size.
                const size = destinationFor(call, image, outcome).sizes[level];
                const fmt = readFormat(gl, "RGBA", "UNSIGNED_BYTE");
                const read = Sink.read(gl, texture, { target: call.target, level, layer: 0, width: size.w, height: size.h, fmt });
                if (read.error)
                    failures.push(`readback error ${Sink.errorName(read.error)}`);
                const observed = Sink.observedSize(read.readback);
                if (observed.w !== size.w || observed.h !== size.h)
                    failures.push(`texture changed: size ${observed.w}x${observed.h}, expected ${size.w}x${size.h}`);
                else {
                    const fill = Sink.compareFill(read.readback, size.w, size.h, [], fillColor, 1);
                    if (fill)
                        failures.push(`texture changed: ${fill}`);
                }
            }
            return { record, error: result.error, unavailable, failures, readback, call, outcome, image };
        } finally {
            gl.deleteTexture(texture);
        }
    }

    // ---- Reporting ----

    // "hardware (direct), pixel format &8v0", "software (forced)", "no upload record, INVALID_VALUE",
    // "hardware not available (gate-function)".
    function resultDescription(r, options = { })
    {
        if (r.unavailable)
            return `hardware not available (${r.unavailable})`;
        const parts = [r.record ? Sink.recordDescription(r.record) : "no upload record"];
        if (options.pixelFormat && r.record && r.record.pixelFormat)
            parts.push(`pixel format ${r.record.pixelFormat}`);
        if (r.error)
            parts.push(Sink.errorName(r.error));
        return parts.join(", ");
    }

    // One line per case: PASS with the path, or FAIL with the path and the failures. A forced
    // hardware upload that was not possible (and left the texture unchanged) is a plain line.
    function reportCase(label, r, options = { })
    {
        const description = resultDescription(r, options);
        if (r.failures.length)
            testFailed(`${label}: ${description}: ${r.failures.join("; ")}`);
        else if (r.unavailable)
            debug(escapeHTML(`${label}: ${description}`));
        else
            testPassed(`${label}: ${description}`);
    }

    // Compares the forced hardware and forced software results of the same upload per patch. The
    // tolerance is 4 when the hardware path used the frame directly and 5 when it converted it, plus
    // the quantisation of the format.
    function reportHardwareVsSoftware(label, hardware, software)
    {
        if (!hardware.readback || !software.readback) {
            const why = hardware.unavailable ? `hardware not available (${hardware.unavailable})` : !hardware.readback ? "no hardware upload" : "no software upload";
            debug(escapeHTML(`${label}: not compared, ${why}`));
            return;
        }
        const fmt = hardware.outcome.fmt;
        const { tolerance, alphaTolerance } = Sink.formatTolerance(fmt, hardware.record && hardware.record.reason === "converted" ? 5 : 4);
        let worst = null;
        for (const region of hardware.outcome.regions) {
            for (const patch of Sink.regionPatches(hardware.image, hardware.call, region, fmt)) {
                const valueH = VF.samplePatch(hardware.readback, patch.rect);
                const valueS = VF.samplePatch(software.readback, patch.rect);
                for (const k of patch.checkColor ? [0, 1, 2, 3] : [3]) {
                    const difference = Math.abs(valueH[k] - valueS[k]);
                    if (difference > (k === 3 ? alphaTolerance : tolerance) && (!worst || difference > worst.difference))
                        worst = { difference, patch, valueH, valueS };
                }
            }
        }
        if (worst)
            testFailed(`${label}: differ by up to ${Math.round(worst.difference)} at ${worst.patch.cell}: hardware ${VF.formatValue(worst.valueH)}, software ${VF.formatValue(worst.valueS)}`);
        else
            testPassed(`${label}: match`);
    }

    // The default, forced software and forced hardware uploads of a plain texImage2D
    // (RGBA/UNSIGNED_BYTE, level 0) of one source, and hardware against software.
    function runPaths(gl, label, source)
    {
        const c = { fn: "texImage2D" };
        reportCase(`${label} default path`, runCase(gl, source, c, "default"), { pixelFormat: true });
        const software = runCase(gl, source, c, "software");
        reportCase(`${label} software path`, software);
        const hardware = runCase(gl, source, c, "hardware");
        reportCase(`${label} hardware path`, hardware);
        reportHardwareVsSoftware(`${label} hardware vs software`, hardware, software);
    }

    // ---- Tests ----

    // One video (the file, or played through MediaSource): the paths of the video element and of
    // VideoFrame(video), in one WebGL 2 context.
    async function runVideoTest(spec)
    {
        const name = spec.video;
        const label = name + (spec.mse ? " (MSE)" : "");
        debug(escapeHTML(videoDescription(name)));
        let video;
        try {
            video = await VF.loadVideo(name, { mse: spec.mse });
        } catch (e) {
            testFailed(`${label}: could not load: ${e.message}`);
            return;
        }
        const sources = [videoSource(video, name)];
        try {
            sources.push(frameOfVideoSource(video, name));
        } catch (e) {
            testFailed(`${label} VideoFrame(video): ${e.name}: ${e.message}`);
        }
        try {
            withContext(2, gl => {
                for (const source of sources)
                    runPaths(gl, `${label} ${source.label}`, source);
            });
        } finally {
            sources.forEach(s => s.close());
            VF.releaseVideo(video);
        }
    }

    // The first VideoDecoder output frame of each video, with prefer-hardware and prefer-software:
    // the same paths as runVideoTest(), in one WebGL 2 context.
    async function runDecoderTest(spec)
    {
        let gl = Sink.createContext(2);
        try {
            for (const name of spec.videos) {
                const data = await VF.fetchArrayBuffer(VF.videoURL(name));
                for (const acceleration of ["prefer-hardware", "prefer-software"]) {
                    let frames;
                    try {
                        frames = await VF.decodeFrames(name, { hardwareAcceleration: acceleration }, { data, chunks: 1 });
                    } catch (e) {
                        debug(escapeHTML(`${name} VideoDecoder ${acceleration}: ${e.name}: ${e.message}`));
                        continue;
                    }
                    if (!frames.length) {
                        testFailed(`${name} VideoDecoder ${acceleration}: no output frame`);
                        continue;
                    }
                    const source = decoderSource(frames.shift(), name, acceleration);
                    frames.forEach(f => f.close());
                    try {
                        runPaths(gl, `${name} ${source.label}`, source);
                    } finally {
                        source.close();
                    }
                }
            }
        } finally {
            Sink.releaseContext(gl);
            gl = null;
            Sink.collectGarbage();
        }
    }

    // The gate conditions of the hardware path, with the default path. Returns [{c, label}].
    function apiCases(gl, image)
    {
        const cases = [];
        const webgl2 = Sink.isWebGL2(gl);
        // Every (internalformat, format, type) that the gate lets through.
        for (const triple of Sink.triples(gl).filter(t => (t.format === "RGB" || t.format === "RGBA") && t.type === "UNSIGNED_BYTE"))
            cases.push({ triple });
        // A combination the gate does not let through.
        cases.push({ triple: webgl2 ? { internalformat: "RGBA16F", format: "RGBA", type: "HALF_FLOAT" } : { internalformat: "RGBA", format: "RGBA", type: "UNSIGNED_SHORT_4_4_4_4" } });
        // Other upload functions.
        cases.push({ fn: "texSubImage2D" });
        if (webgl2)
            cases.push({ fn: "texImage3D", target: "TEXTURE_2D_ARRAY" });
        // A source sub-rectangle.
        const cell = 64;
        if (webgl2)
            cases.push({ rect: { x: cell, y: cell, w: image.width - 2 * cell, h: image.height - cell } });
        // Level 1 (WebGL 1: INVALID_VALUE for a non-power-of-two image).
        cases.push({ level: 1 });
        // A cube map face: the whole image (INVALID_VALUE: not square) and, in WebGL 2, a square
        // sub-rectangle.
        cases.push({ target: "TEXTURE_CUBE_MAP_POSITIVE_X" });
        if (webgl2) {
            const side = Math.min(image.width, image.height);
            cases.push({ target: "TEXTURE_CUBE_MAP_POSITIVE_X", rect: { x: Math.floor((image.width - side) / 2 / cell) * cell, y: 0, w: side, h: side } });
        }
        return cases;
    }

    // spec: {videos: [direct, converted], frames: [VideoFrame buffer formats]}
    async function runApiTest(spec)
    {
        for (const name of spec.videos)
            debug(escapeHTML(videoDescription(name)));
        const videos = await Promise.all(spec.videos.map(name => VF.loadVideo(name)));
        const sources = videos.map((video, i) => videoSource(video, spec.videos[i]));
        sources.forEach((source, i) => { source.label = `${spec.videos[i]} ${source.label}`; });
        try {
            for (const format of spec.frames)
                sources.push(patternFrameSource(format));
            for (const version of [1, 2]) {
                withContext(version, gl => {
                    for (const source of sources) {
                        for (const c of apiCases(gl, source.image))
                            reportCase(`WebGL ${version} ${source.label} ${caseLabel(c)}`, runCase(gl, source, c));
                    }
                });
            }
        } finally {
            sources.forEach(s => s.close());
            videos.forEach(video => VF.releaseVideo(video));
        }
    }

    // sRGB internal formats with the default, forced hardware and forced software paths.
    // spec: {videos}
    async function runSrgbTest(spec)
    {
        for (const name of spec.videos)
            debug(escapeHTML(videoDescription(name)));
        const videos = await Promise.all(spec.videos.map(name => VF.loadVideo(name)));
        const sources = videos.map((video, i) => videoSource(video, spec.videos[i]));
        sources.forEach((source, i) => { source.label = `${spec.videos[i]} ${source.label}`; });
        try {
            for (const version of [1, 2]) {
                withContext(version, gl => {
                    let triples;
                    if (version === 2)
                        triples = [{ internalformat: "SRGB8_ALPHA8", format: "RGBA", type: "UNSIGNED_BYTE" }, { internalformat: "SRGB8", format: "RGB", type: "UNSIGNED_BYTE" }];
                    else if (gl.getExtension("EXT_sRGB"))
                        triples = [{ internalformat: "SRGB_ALPHA_EXT", format: "SRGB_ALPHA_EXT", type: "UNSIGNED_BYTE" }, { internalformat: "SRGB_EXT", format: "SRGB_EXT", type: "UNSIGNED_BYTE" }];
                    else {
                        debug("WebGL 1: EXT_sRGB is not exposed");
                        return;
                    }
                    for (const source of sources) {
                        for (const triple of triples) {
                            const c = { triple };
                            for (const path of ["default", "hardware", "software"])
                                reportCase(`WebGL ${version} ${source.label} ${caseLabel(c)} ${path} path`, runCase(gl, source, c, path));
                        }
                    }
                });
            }
        } finally {
            videos.forEach(video => VF.releaseVideo(video));
        }
    }

    return {
        run,
        runVideoTest,
        runDecoderTest,
        runApiTest,
        runSrgbTest,
    };
})();
