// WebGL sink for the video-formats tests: uploads video sources (video element, VideoFrame) into
// textures with texImage2D, texSubImage2D, texImage3D and texSubImage3D, and reads the texture back. A
// sink is the web API that consumes a video frame; see video-formats-test-utils.js for the term, the
// sink interface and the shared oracle.
//
// What this file provides:
// * contexts, enum names, the TexImageSource format table (internalformat, format, type) for WebGL 1
//   and WebGL 2, and format compatibility for sub-image uploads;
// * destination set-up (mutable or immutable levels, cube faces, layers) filled with known colours;
// * uploads (texImage2D, texSubImage2D, texImage3D, texSubImage3D, plain and sized overloads) with
//   unpack parameters;
// * readback of one level / face / layer into an 8-bit RGBA readback one pixel larger than the
//   level in each direction, pre-filled with a sentinel, so that a level of the wrong size shows up.
//   Colour-renderable normalised formats are read with a framebuffer and readPixels (raw stored
//   values, also for sRGB). Other formats are drawn with texelFetch into an RGBA8 target (integer
//   formats with an unsigned-integer sampler, sRGB formats re-encoded); WebGL 1 samples at texel
//   centres.
// * the oracle for a format: the shared patch values (native R'G'B', no colour management), mapped
//   to the channels the format keeps and quantised to its bit depth;
// * the upload record of the default/software/hardware upload path, when the test hooks exist
//   (internals.webglLastVideoUpload / internals.setWebGLVideoUploadPath). Without them nothing
//   path-related is reported.
// * context release: canvas out of the DOM, references dropped, garbage collection.

"use strict";

var WebGLVideoSink = (() => {
    const SENTINEL = [1, 2, 3, 4];

    const extensionEnums = {
        HALF_FLOAT_OES: 0x8D61,
        SRGB_EXT: 0x8C40,
        SRGB_ALPHA_EXT: 0x8C42,
    };

    function enumValue(gl, name)
    {
        if (typeof name === "number")
            return name;
        if (name in extensionEnums)
            return extensionEnums[name];
        const value = gl[name];
        if (value === undefined)
            throw new Error("unknown enum " + name);
        return value;
    }

    const errorNames = { 0: "NO_ERROR", 0x0500: "INVALID_ENUM", 0x0501: "INVALID_VALUE", 0x0502: "INVALID_OPERATION", 0x0505: "OUT_OF_MEMORY", 0x0506: "INVALID_FRAMEBUFFER_OPERATION", 0x9242: "CONTEXT_LOST_WEBGL" };

    function errorName(value)
    {
        return errorNames[value] || ("0x" + value.toString(16));
    }

    function isWebGL2(gl)
    {
        return window.WebGL2RenderingContext && gl instanceof WebGL2RenderingContext;
    }

    function createContext(version, attributes = { })
    {
        let canvas = document.createElement("canvas");
        canvas.width = 16;
        canvas.height = 16;
        const gl = canvas.getContext(version === 2 ? "webgl2" : "webgl", attributes);
        canvas = null;
        if (!gl)
            throw new Error(`could not create a WebGL ${version} context`);
        createdContexts.push(gl);
        return gl;
    }

    // Contexts stay alive until garbage collection. They are released the way a page releases them: at
    // the end of its use the canvas leaves the DOM, the test drops its references (its variables are
    // set to null) and a garbage collection runs. Usage:
    //     Sink.releaseContext(gl);
    //     gl = null;
    //     Sink.collectGarbage();
    const createdContexts = [];

    function releaseContext(gl)
    {
        if (!gl)
            return;
        const index = createdContexts.indexOf(gl);
        if (index >= 0)
            createdContexts.splice(index, 1);
        if (gl.canvas && gl.canvas.parentNode)
            gl.canvas.remove();
    }

    function collectGarbage()
    {
        if (window.gc)
            gc();
    }

    // At the end of a test: releases the contexts the test did not release itself.
    function releaseContexts()
    {
        for (const gl of createdContexts.splice(0)) {
            if (gl.canvas && gl.canvas.parentNode)
                gl.canvas.remove();
        }
        collectGarbage();
    }

    function disableConsoleErrors()
    {
        if (window.internals && internals.settings && internals.settings.setWebGLErrorsToConsoleEnabled)
            internals.settings.setWebGLErrorsToConsoleEnabled(false);
    }

    // Drains the error queue. Returns the first error (or NO_ERROR).
    function takeError(gl)
    {
        const first = gl.getError();
        for (let i = 0; i < 8 && gl.getError(); ++i) { }
        return first;
    }

    // ---- Formats ----

    // Every (internalformat, format, type) that WebGL 2 accepts for TexImageSource uploads.
    const webgl2Triples = [
        ["RGB", "RGB", "UNSIGNED_BYTE"], ["RGB", "RGB", "UNSIGNED_SHORT_5_6_5"],
        ["RGBA", "RGBA", "UNSIGNED_BYTE"], ["RGBA", "RGBA", "UNSIGNED_SHORT_4_4_4_4"], ["RGBA", "RGBA", "UNSIGNED_SHORT_5_5_5_1"],
        ["LUMINANCE_ALPHA", "LUMINANCE_ALPHA", "UNSIGNED_BYTE"], ["LUMINANCE", "LUMINANCE", "UNSIGNED_BYTE"], ["ALPHA", "ALPHA", "UNSIGNED_BYTE"],
        ["R8", "RED", "UNSIGNED_BYTE"], ["R16F", "RED", "HALF_FLOAT"], ["R16F", "RED", "FLOAT"], ["R32F", "RED", "FLOAT"],
        ["R8UI", "RED_INTEGER", "UNSIGNED_BYTE"],
        ["RG8", "RG", "UNSIGNED_BYTE"], ["RG16F", "RG", "HALF_FLOAT"], ["RG16F", "RG", "FLOAT"], ["RG32F", "RG", "FLOAT"],
        ["RG8UI", "RG_INTEGER", "UNSIGNED_BYTE"],
        ["RGB8", "RGB", "UNSIGNED_BYTE"], ["SRGB8", "RGB", "UNSIGNED_BYTE"],
        ["RGB565", "RGB", "UNSIGNED_BYTE"], ["RGB565", "RGB", "UNSIGNED_SHORT_5_6_5"],
        ["R11F_G11F_B10F", "RGB", "UNSIGNED_INT_10F_11F_11F_REV"], ["R11F_G11F_B10F", "RGB", "HALF_FLOAT"], ["R11F_G11F_B10F", "RGB", "FLOAT"],
        ["RGB9_E5", "RGB", "HALF_FLOAT"], ["RGB9_E5", "RGB", "FLOAT"],
        ["RGB16F", "RGB", "HALF_FLOAT"], ["RGB16F", "RGB", "FLOAT"], ["RGB32F", "RGB", "FLOAT"],
        ["RGB8UI", "RGB_INTEGER", "UNSIGNED_BYTE"],
        ["RGBA8", "RGBA", "UNSIGNED_BYTE"], ["SRGB8_ALPHA8", "RGBA", "UNSIGNED_BYTE"],
        ["RGB5_A1", "RGBA", "UNSIGNED_BYTE"], ["RGB5_A1", "RGBA", "UNSIGNED_SHORT_5_5_5_1"],
        ["RGB10_A2", "RGBA", "UNSIGNED_INT_2_10_10_10_REV"],
        ["RGBA4", "RGBA", "UNSIGNED_BYTE"], ["RGBA4", "RGBA", "UNSIGNED_SHORT_4_4_4_4"],
        ["RGBA16F", "RGBA", "HALF_FLOAT"], ["RGBA16F", "RGBA", "FLOAT"], ["RGBA32F", "RGBA", "FLOAT"],
        ["RGBA8UI", "RGBA_INTEGER", "UNSIGNED_BYTE"],
    ].map(([internalformat, format, type]) => ({ internalformat, format, type }));

    // WebGL 1: internalformat == format; float types and sRGB through extensions.
    function webgl1Triples(gl)
    {
        const triples = [
            ["RGB", "UNSIGNED_BYTE"], ["RGB", "UNSIGNED_SHORT_5_6_5"],
            ["RGBA", "UNSIGNED_BYTE"], ["RGBA", "UNSIGNED_SHORT_4_4_4_4"], ["RGBA", "UNSIGNED_SHORT_5_5_5_1"],
            ["LUMINANCE", "UNSIGNED_BYTE"], ["LUMINANCE_ALPHA", "UNSIGNED_BYTE"], ["ALPHA", "UNSIGNED_BYTE"],
        ];
        if (gl.getExtension("OES_texture_float")) {
            for (const format of ["RGB", "RGBA", "LUMINANCE", "LUMINANCE_ALPHA", "ALPHA"])
                triples.push([format, "FLOAT"]);
        }
        if (gl.getExtension("OES_texture_half_float")) {
            for (const format of ["RGB", "RGBA", "LUMINANCE", "LUMINANCE_ALPHA", "ALPHA"])
                triples.push([format, "HALF_FLOAT_OES"]);
        }
        if (gl.getExtension("EXT_sRGB")) {
            triples.push(["SRGB_EXT", "UNSIGNED_BYTE"]);
            triples.push(["SRGB_ALPHA_EXT", "UNSIGNED_BYTE"]);
        }
        return triples.map(([format, type]) => ({ internalformat: format, format, type }));
    }

    function triples(gl)
    {
        return isWebGL2(gl) ? webgl2Triples : webgl1Triples(gl);
    }

    // The sized internal format with which texStorage defines a level like `triple`, or null.
    function sizedFormatOf(triple)
    {
        if (!unsizedFormats.includes(triple.internalformat))
            return triple.internalformat;
        const table = {
            "RGBA/UNSIGNED_BYTE": "RGBA8", "RGB/UNSIGNED_BYTE": "RGB8", "RGB/UNSIGNED_SHORT_5_6_5": "RGB565",
            "RGBA/UNSIGNED_SHORT_4_4_4_4": "RGBA4", "RGBA/UNSIGNED_SHORT_5_5_5_1": "RGB5_A1",
        };
        return table[`${triple.internalformat}/${triple.type}`] || null;
    }

    function tripleLabel(t)
    {
        return t.internalformat === t.format ? `${t.format}/${t.type}` : `${t.internalformat}/${t.format}/${t.type}`;
    }

    const unsizedFormats = ["RGB", "RGBA", "LUMINANCE", "LUMINANCE_ALPHA", "ALPHA", "SRGB_EXT", "SRGB_ALPHA_EXT"];

    // Compatibility of a sub-image upload (format, type) with a level defined by `level`
    // ({internalformat, format, type}). Returns "yes", "no" or "either" (the specs leave it open
    // or engines differ; both an error and an upload are accepted).
    function subImageCompatibility(gl, level, format, type)
    {
        if (unsizedFormats.includes(level.internalformat)) {
            if (format !== level.format)
                return "no";
            if (type === level.type)
                return "yes";
            // Unsized level, same format, another type: ES 3.0 table 3.3 lists the pair, but the
            // level has an effective sized format, and WebGL 1 / ES 2.0 do not say.
            return triples(gl).some(t => t.internalformat === level.internalformat && t.format === format && t.type === type) ? "either" : "no";
        }
        const valid = webgl2Triples.some(t => t.internalformat === level.internalformat && t.format === format && t.type === type);
        return valid ? "yes" : "no";
    }

    const channelsByFormat = {
        RGBA: "rgba", RGBA8: "rgba", SRGB8_ALPHA8: "rgba", SRGB_ALPHA_EXT: "rgba", RGB5_A1: "rgba", RGB10_A2: "rgba", RGBA4: "rgba",
        RGBA16F: "rgba", RGBA32F: "rgba", RGBA8UI: "rgba",
        RGB: "rgb", RGB8: "rgb", SRGB8: "rgb", SRGB_EXT: "rgb", RGB565: "rgb", R11F_G11F_B10F: "rgb", RGB9_E5: "rgb", RGB16F: "rgb",
        RGB32F: "rgb", RGB8UI: "rgb",
        RG8: "rg", RG16F: "rg", RG32F: "rg", RG8UI: "rg",
        R8: "r", R16F: "r", R32F: "r", R8UI: "r",
        LUMINANCE: "l", LUMINANCE_ALPHA: "la", ALPHA: "a",
    };

    // Describes how a level of `internalformat` (defined or uploaded with `type`) stores and reads back.
    //   channels: rgba | rgb | rg | r | l | la | a
    //   bits: [r, g, b, a] for quantised normalised formats, null for 8-bit and float
    //   read: "fbo" | "sample" | "uint"
    //   srgb: sRGB-encoded storage (the sample path re-encodes after the hardware decode)
    //   extraTolerance: float formats with few mantissa bits
    function formatInfo(gl, internalformat, type)
    {
        const channels = channelsByFormat[internalformat];
        if (!channels)
            throw new Error("unknown internalformat " + internalformat);
        let bits = null;
        if (internalformat === "RGB565" || (internalformat === "RGB" && type === "UNSIGNED_SHORT_5_6_5"))
            bits = [5, 6, 5, 0];
        else if (internalformat === "RGBA4" || (internalformat === "RGBA" && type === "UNSIGNED_SHORT_4_4_4_4"))
            bits = [4, 4, 4, 4];
        else if (internalformat === "RGB5_A1" || (internalformat === "RGBA" && type === "UNSIGNED_SHORT_5_5_5_1"))
            bits = [5, 5, 5, 1];
        else if (internalformat === "RGB10_A2")
            bits = [10, 10, 10, 2];
        const isFloatType = ["FLOAT", "HALF_FLOAT", "HALF_FLOAT_OES"].includes(type);
        let read = "sample";
        if (internalformat.endsWith("UI"))
            read = "uint";
        else if (isWebGL2(gl)) {
            if (["RGBA8", "RGB8", "RGB565", "RGBA4", "RGB5_A1", "RGB10_A2", "SRGB8_ALPHA8", "R8", "RG8"].includes(internalformat))
                read = "fbo";
            else if ((internalformat === "RGBA" || internalformat === "RGB") && !isFloatType)
                read = "fbo";
        } else if ((internalformat === "RGBA" || internalformat === "RGB") && !isFloatType)
            read = "fbo";
        const srgb = ["SRGB8", "SRGB8_ALPHA8", "SRGB_EXT", "SRGB_ALPHA_EXT"].includes(internalformat);
        let extraTolerance = 0;
        if (internalformat === "R11F_G11F_B10F")
            extraTolerance = 4;
        else if (internalformat === "RGB9_E5" || srgb)
            extraTolerance = 1;
        return { internalformat, type, channels, bits, read, srgb, extraTolerance };
    }

    function quantise(value, bits)
    {
        if (!bits)
            return value;
        const max = (1 << bits) - 1;
        return Math.round(value / 255 * max) * 255 / max;
    }

    // Maps a straight or premultiplied source RGBA (0-255) to what the readback of a level of format
    // `fmt` holds.
    function storedValue(fmt, rgba)
    {
        let [r, g, b, a] = rgba;
        if (fmt.bits) {
            r = quantise(r, fmt.bits[0]);
            g = quantise(g, fmt.bits[1]);
            b = quantise(b, fmt.bits[2]);
            a = fmt.bits[3] ? quantise(a, fmt.bits[3]) : 255;
        }
        switch (fmt.channels) {
        case "rgba": return [r, g, b, a];
        case "rgb": return [r, g, b, 255];
        case "rg": return [r, g, 0, 255];
        case "r": return [r, 0, 0, 255];
        case "l": return [r, r, r, 255];
        case "la": return [r, r, r, a];
        case "a": return [0, 0, 0, a];
        }
        throw new Error("bad channels");
    }

    // Per-channel tolerance for a format, on top of the source tolerance.
    function formatTolerance(fmt, base)
    {
        let tolerance = base + fmt.extraTolerance;
        let alphaTolerance = Math.max(2, tolerance);
        if (fmt.bits) {
            const quantum = Math.max(...fmt.bits.slice(0, 3).map(b => 255 / ((1 << b) - 1)));
            tolerance = Math.max(tolerance, Math.ceil(quantum));
            if (fmt.bits[3] >= 2)
                alphaTolerance = Math.max(alphaTolerance, Math.ceil(255 / ((1 << fmt.bits[3]) - 1)));
        }
        return { tolerance, alphaTolerance };
    }

    // Maps expectedPatches() output to the format.
    function patchesForFormat(fmt, patches)
    {
        return patches.map(p => {
            if (!p.expected)
                return p;
            const mapped = Object.assign({ }, p, { expected: storedValue(fmt, p.expected) });
            // A colour that is undefined at alpha 0 (unpremultiplied) stays unchecked only where the
            // format keeps the colour channels.
            if (!p.checkColor && fmt.channels === "a")
                mapped.checkColor = true;
            return mapped;
        });
    }

    // ---- Fill data ----

    function channelValues(format, rgba)
    {
        const [r, g, b, a] = rgba;
        switch (format) {
        case "RGBA": case "RGBA_INTEGER": case "SRGB_ALPHA_EXT": return [r, g, b, a];
        case "RGB": case "RGB_INTEGER": case "SRGB_EXT": return [r, g, b];
        case "RG": case "RG_INTEGER": return [r, g];
        case "RED": case "RED_INTEGER": case "LUMINANCE": return [r];
        case "LUMINANCE_ALPHA": return [r, a];
        case "ALPHA": return [a];
        }
        throw new Error("bad format " + format);
    }

    function toHalf(value)
    {
        const f32 = new Float32Array(1);
        const u32 = new Uint32Array(f32.buffer);
        f32[0] = value;
        const x = u32[0];
        const sign = (x >> 16) & 0x8000;
        let exponent = ((x >> 23) & 0xff) - 127 + 15;
        let mantissa = x & 0x7fffff;
        if (exponent <= 0)
            return sign;
        if (exponent >= 31)
            return sign | 0x7c00;
        // Round to nearest.
        mantissa += 0x1000;
        if (mantissa & 0x800000) {
            mantissa = 0;
            ++exponent;
        }
        return sign | (exponent << 10) | (mantissa >> 13);
    }

    // Repeats the first `unit` elements of `data` over the whole array (native copies; per-element
    // loops are slow in debug builds).
    function repeatFill(data, unit)
    {
        let filled = unit;
        while (filled < data.length) {
            const count = Math.min(filled, data.length - filled);
            data.copyWithin(filled, 0, count);
            filled += count;
        }
        return data;
    }

    // Typed data for a w x h x d block of one colour, for (format, type).
    function fillData(format, type, w, h, d, rgba)
    {
        const n = w * h * (d || 1);
        const values = channelValues(format, rgba);
        const c = values.length;
        const q = (v, bits) => Math.round(v / 255 * ((1 << bits) - 1));
        let data;
        let unit;
        switch (type) {
        case "UNSIGNED_BYTE":
            data = new Uint8Array(n * c);
            data.set(values);
            unit = c;
            break;
        case "FLOAT":
            data = new Float32Array(n * c);
            data.set(values.map(v => v / 255));
            unit = c;
            break;
        case "HALF_FLOAT": case "HALF_FLOAT_OES":
            data = new Uint16Array(n * c);
            data.set(values.map(v => toHalf(v / 255)));
            unit = c;
            break;
        case "UNSIGNED_SHORT_5_6_5":
            return new Uint16Array(n).fill((q(values[0], 5) << 11) | (q(values[1], 6) << 5) | q(values[2], 5));
        case "UNSIGNED_SHORT_4_4_4_4":
            return new Uint16Array(n).fill((q(values[0], 4) << 12) | (q(values[1], 4) << 8) | (q(values[2], 4) << 4) | q(values[3], 4));
        case "UNSIGNED_SHORT_5_5_5_1":
            return new Uint16Array(n).fill((q(values[0], 5) << 11) | (q(values[1], 5) << 6) | (q(values[2], 5) << 1) | q(values[3], 1));
        case "UNSIGNED_INT_2_10_10_10_REV":
            return new Uint32Array(n).fill((q(values[0], 10) | (q(values[1], 10) << 10) | (q(values[2], 10) << 20) | (q(values[3], 2) << 30)) >>> 0);
        default:
            throw new Error("no fill data for " + type);
        }
        return n ? repeatFill(data, unit) : data;
    }

    // The (format, type) used to fill a level of a format that was defined or uploaded with `type`.
    function fillTriple(triple)
    {
        if (triple.type === "UNSIGNED_INT_10F_11F_11F_REV")
            return { internalformat: triple.internalformat, format: triple.format, type: "FLOAT" };
        return triple;
    }

    // ---- Unpack state ----

    function resetUnpack(gl)
    {
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
        gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
        gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.BROWSER_DEFAULT_WEBGL);
        gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
        if (isWebGL2(gl)) {
            gl.pixelStorei(gl.UNPACK_ROW_LENGTH, 0);
            gl.pixelStorei(gl.UNPACK_SKIP_PIXELS, 0);
            gl.pixelStorei(gl.UNPACK_SKIP_ROWS, 0);
            gl.pixelStorei(gl.UNPACK_IMAGE_HEIGHT, 0);
            gl.pixelStorei(gl.UNPACK_SKIP_IMAGES, 0);
        }
    }

    function applyUnpack(gl, unpack = { })
    {
        if (unpack.flipY)
            gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
        if (unpack.premultiply)
            gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
        if (unpack.colorspaceConversion !== undefined)
            gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, enumValue(gl, unpack.colorspaceConversion));
        if (unpack.alignment)
            gl.pixelStorei(gl.UNPACK_ALIGNMENT, unpack.alignment);
        if (unpack.rowLength)
            gl.pixelStorei(gl.UNPACK_ROW_LENGTH, unpack.rowLength);
        if (unpack.skipPixels)
            gl.pixelStorei(gl.UNPACK_SKIP_PIXELS, unpack.skipPixels);
        if (unpack.skipRows)
            gl.pixelStorei(gl.UNPACK_SKIP_ROWS, unpack.skipRows);
        if (unpack.imageHeight)
            gl.pixelStorei(gl.UNPACK_IMAGE_HEIGHT, unpack.imageHeight);
        if (unpack.skipImages)
            gl.pixelStorei(gl.UNPACK_SKIP_IMAGES, unpack.skipImages);
    }

    // ---- Targets ----

    const cubeFaces = ["TEXTURE_CUBE_MAP_POSITIVE_X", "TEXTURE_CUBE_MAP_NEGATIVE_X", "TEXTURE_CUBE_MAP_POSITIVE_Y",
        "TEXTURE_CUBE_MAP_NEGATIVE_Y", "TEXTURE_CUBE_MAP_POSITIVE_Z", "TEXTURE_CUBE_MAP_NEGATIVE_Z"];

    function bindTargetOf(target)
    {
        return cubeFaces.includes(target) ? "TEXTURE_CUBE_MAP" : target;
    }

    function isLayered(bindTarget)
    {
        return bindTarget === "TEXTURE_2D_ARRAY" || bindTarget === "TEXTURE_3D";
    }

    function shortTarget(target)
    {
        if (cubeFaces.includes(target))
            return target.replace("TEXTURE_CUBE_MAP_", "cube ").replace("POSITIVE_", "+").replace("NEGATIVE_", "-");
        return target.replace("TEXTURE_", "");
    }

    // ---- Destinations ----

    // dest: {
    //   bindTarget, internalformat, format, type (the defining triple),
    //   immutable, levels (immutable level count),
    //   sizes: [{w, h, d} | null] per level (immutable: level 0 size, the rest derived),
    //   fill(level, faceOrLayer) -> rgba
    // }
    function levelSize(dest, level)
    {
        if (dest.immutable) {
            if (level >= dest.levels)
                return null;
            const base = dest.sizes[0];
            const d = dest.bindTarget === "TEXTURE_3D" ? Math.max(1, base.d >> level) : (base.d || 1);
            return { w: Math.max(1, base.w >> level), h: Math.max(1, base.h >> level), d };
        }
        return dest.sizes[level] || null;
    }

    function faceTargets(dest)
    {
        return dest.bindTarget === "TEXTURE_CUBE_MAP" ? cubeFaces : [dest.bindTarget];
    }

    function defineDestination(gl, texture, dest)
    {
        resetUnpack(gl);
        // The fill data is tightly packed, also for odd widths (e.g. a 321x321 cube face).
        gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
        gl.activeTexture(gl.TEXTURE0);
        const bindTarget = enumValue(gl, dest.bindTarget);
        gl.bindTexture(bindTarget, texture);
        const fill = fillTriple(dest);
        const ifmt = enumValue(gl, dest.internalformat);
        const format = enumValue(gl, fill.format);
        const type = enumValue(gl, fill.type);
        const layered = isLayered(dest.bindTarget);
        if (dest.immutable) {
            const s = dest.sizes[0];
            if (layered)
                gl.texStorage3D(bindTarget, dest.levels, ifmt, s.w, s.h, s.d);
            else
                gl.texStorage2D(bindTarget, dest.levels, ifmt, s.w, s.h);
        }
        const levelCount = dest.immutable ? dest.levels : dest.sizes.length;
        for (let level = 0; level < levelCount; ++level) {
            const s = levelSize(dest, level);
            if (!s)
                continue;
            if (layered) {
                if (!dest.immutable)
                    gl.texImage3D(bindTarget, level, ifmt, s.w, s.h, s.d, 0, format, type, null);
                for (let z = 0; z < s.d; ++z)
                    gl.texSubImage3D(bindTarget, level, 0, 0, z, s.w, s.h, 1, format, type, fillData(fill.format, fill.type, s.w, s.h, 1, dest.fill(level, z)));
            } else {
                faceTargets(dest).forEach((face, i) => {
                    // dest.faceDefinition(level, face) can give a face its own size and format.
                    const own = !dest.immutable && dest.faceDefinition ? dest.faceDefinition(level, i) : null;
                    if (own) {
                        const ownFill = fillTriple(own);
                        gl.texImage2D(enumValue(gl, face), level, enumValue(gl, own.internalformat), own.w, own.h, 0, enumValue(gl, ownFill.format), enumValue(gl, ownFill.type),
                            fillData(ownFill.format, ownFill.type, own.w, own.h, 1, dest.fill(level, i)));
                        return;
                    }
                    const data = fillData(fill.format, fill.type, s.w, s.h, 1, dest.fill(level, i));
                    if (dest.immutable)
                        gl.texSubImage2D(enumValue(gl, face), level, 0, 0, s.w, s.h, format, type, data);
                    else
                        gl.texImage2D(enumValue(gl, face), level, ifmt, s.w, s.h, 0, format, type, data);
                });
            }
        }
        return takeError(gl);
    }

    // ---- Uploads ----

    // call: {fn, target, level, internalformat, format, type, x, y, z, width, height, depth, unpack}
    // width/height present = the sized overload (WebGL 2).
    function invoke(gl, call, image)
    {
        const target = enumValue(gl, call.target);
        const format = enumValue(gl, call.format);
        const type = enumValue(gl, call.type);
        const level = call.level || 0;
        switch (call.fn) {
        case "texImage2D":
            if (call.width !== undefined)
                gl.texImage2D(target, level, enumValue(gl, call.internalformat), call.width, call.height, 0, format, type, image);
            else
                gl.texImage2D(target, level, enumValue(gl, call.internalformat), format, type, image);
            break;
        case "texSubImage2D":
            if (call.width !== undefined)
                gl.texSubImage2D(target, level, call.x || 0, call.y || 0, call.width, call.height, format, type, image);
            else
                gl.texSubImage2D(target, level, call.x || 0, call.y || 0, format, type, image);
            break;
        case "texImage3D":
            gl.texImage3D(target, level, enumValue(gl, call.internalformat), call.width, call.height, call.depth || 1, 0, format, type, image);
            break;
        case "texSubImage3D":
            gl.texSubImage3D(target, level, call.x || 0, call.y || 0, call.z || 0, call.width, call.height, call.depth || 1, format, type, image);
            break;
        default:
            throw new Error("unknown upload function " + call.fn);
        }
    }

    // Uploads `image` into `texture` (bound to unit 0). Returns {error, exception, record}.
    function upload(gl, texture, call, image)
    {
        takeError(gl);
        resetUnpack(gl);
        applyUnpack(gl, call.unpack);
        gl.activeTexture(gl.TEXTURE0);
        if (texture !== undefined)
            gl.bindTexture(enumValue(gl, bindTargetOf(call.target)), texture);
        let exception = null;
        try {
            invoke(gl, call, image);
        } catch (e) {
            exception = e;
        }
        const error = takeError(gl);
        resetUnpack(gl);
        return { error, exception, record: uploadRecord(gl) };
    }

    function callLabel(call)
    {
        const parts = [];
        if (call.target && call.target !== "TEXTURE_2D")
            parts.push(shortTarget(call.target));
        parts.push(`level ${call.level || 0}`);
        if (call.fn.startsWith("texSub"))
            parts.push(call.fn.endsWith("3D") ? `at ${call.x || 0},${call.y || 0},${call.z || 0}` : `at ${call.x || 0},${call.y || 0}`);
        if (call.width !== undefined) {
            const u = call.unpack || { };
            parts.push(`source ${u.skipPixels || 0},${u.skipRows || 0} ${call.width}x${call.height}` + (call.fn.endsWith("3D") ? `x${call.depth || 1}` : ""));
            if (u.imageHeight)
                parts.push(`image height ${u.imageHeight}`);
            if (u.skipImages)
                parts.push(`skip images ${u.skipImages}`);
        }
        const u = call.unpack || { };
        if (u.flipY)
            parts.push("flipY");
        if (u.premultiply)
            parts.push("premultiply");
        if (u.rowLength)
            parts.push(`row length ${u.rowLength}`);
        if (u.alignment)
            parts.push(`alignment ${u.alignment}`);
        return `${call.fn}(${parts.join(", ")})`;
    }

    // ---- Readback ----

    const contextState = new WeakMap;

    function stateOf(gl)
    {
        let state = contextState.get(gl);
        if (!state) {
            state = { programs: { }, framebuffer: gl.createFramebuffer() };
            contextState.set(gl, state);
        }
        return state;
    }

    const sentinelWord = new Uint32Array(new Uint8Array(SENTINEL).buffer)[0];

    function sentinelReadback(width, height)
    {
        const data = new Uint8Array(width * height * 4);
        new Uint32Array(data.buffer).fill(sentinelWord);
        return { width, height, data };
    }

    function isSentinel(readback, x, y)
    {
        const i = (y * readback.width + x) * 4;
        const d = readback.data;
        return d[i] === SENTINEL[0] && d[i + 1] === SENTINEL[1] && d[i + 2] === SENTINEL[2] && d[i + 3] === SENTINEL[3];
    }

    const vertexShader2 = `#version 300 es
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`;

    const cubeDirection = `
vec3 cubeDirection(int face, vec2 uv) {
    float sc = uv.x;
    float tc = uv.y;
    if (face == 0) return vec3(1.0, -tc, -sc);
    if (face == 1) return vec3(-1.0, -tc, sc);
    if (face == 2) return vec3(sc, 1.0, tc);
    if (face == 3) return vec3(sc, -1.0, -tc);
    if (face == 4) return vec3(sc, -tc, 1.0);
    return vec3(-sc, -tc, -1.0);
}`;

    const srgbEncode = `
vec3 encodeSrgb(vec3 c) {
    c = clamp(c, 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}`;

    function program2(gl, kind, integer)
    {
        const state = stateOf(gl);
        const key = kind + (integer ? "-uint" : "");
        if (state.programs[key])
            return state.programs[key];
        const samplerType = { "2d": "sampler2D", "cube": "samplerCube", "2darray": "sampler2DArray", "3d": "sampler3D" }[kind];
        const sampler = (integer ? "u" : "") + samplerType;
        let fetch;
        if (kind === "2d")
            fetch = "texelFetch(t, p, lod)";
        else if (kind === "cube")
            fetch = "textureLod(t, cubeDirection(face, (vec2(p) + 0.5) / vec2(size) * 2.0 - 1.0), float(lod))";
        else
            fetch = "texelFetch(t, ivec3(p, layer), lod)";
        const source = `#version 300 es
precision highp float;
precision highp int;
uniform highp ${sampler} t;
uniform int layer;
uniform int lod;
uniform int face;
uniform int mode;
uniform int noAlpha;
out vec4 o;
${cubeDirection}
${srgbEncode}
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(t, lod).xy;
    if (p.x >= size.x || p.y >= size.y) {
        o = vec4(${SENTINEL.join(".0, ")}.0) / 255.0;
        return;
    }
    vec4 v = ${integer ? `vec4(${fetch}) / 255.0` : fetch};
    if (noAlpha == 1)
        v.a = 1.0;
    if (mode == 1)
        v.rgb = encodeSrgb(v.rgb);
    o = v;
}`;
        state.programs[key] = linkProgram(gl, vertexShader2, source);
        return state.programs[key];
    }

    const vertexShader1 = `attribute vec2 position;
void main() { gl_Position = vec4(position, 0.0, 1.0); }`;

    function program1(gl, kind)
    {
        const state = stateOf(gl);
        if (state.programs[kind])
            return state.programs[kind];
        const lod = !!gl.getExtension("EXT_shader_texture_lod");
        const sample = kind === "cube"
            ? (lod ? "textureCubeLodEXT(t, cubeDirection(face, uv * 2.0 - 1.0), level)" : "textureCube(t, cubeDirection(face, uv * 2.0 - 1.0))")
            : (lod ? "texture2DLodEXT(t, uv, level)" : "texture2D(t, uv)");
        const source = `${lod ? "#extension GL_EXT_shader_texture_lod : enable" : ""}
precision highp float;
uniform ${kind === "cube" ? "samplerCube" : "sampler2D"} t;
uniform vec2 size;
uniform float level;
uniform int face;
uniform int mode;
${cubeDirection}
${srgbEncode}
void main() {
    vec2 p = gl_FragCoord.xy;
    if (p.x > size.x || p.y > size.y) {
        gl_FragColor = vec4(${SENTINEL.join(".0, ")}.0) / 255.0;
        return;
    }
    vec2 uv = p / size;
    vec4 v = ${sample};
    if (mode == 1)
        v.rgb = encodeSrgb(v.rgb);
    gl_FragColor = v;
}`;
        state.programs[kind] = linkProgram(gl, vertexShader1, source);
        return state.programs[kind];
    }

    function linkProgram(gl, vs, fs)
    {
        const program = gl.createProgram();
        for (const [type, source] of [[gl.VERTEX_SHADER, vs], [gl.FRAGMENT_SHADER, fs]]) {
            const shader = gl.createShader(type);
            gl.shaderSource(shader, source);
            gl.compileShader(shader);
            if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
                throw new Error("readback shader: " + gl.getShaderInfoLog(shader));
            gl.attachShader(program, shader);
        }
        gl.bindAttribLocation(program, 0, "position");
        gl.linkProgram(program);
        if (!gl.getProgramParameter(program, gl.LINK_STATUS))
            throw new Error("readback program: " + gl.getProgramInfoLog(program));
        return program;
    }

    function renderTarget(gl, width, height)
    {
        const state = stateOf(gl);
        if (!state.target) {
            state.target = gl.createTexture();
            state.targetFramebuffer = gl.createFramebuffer();
        }
        gl.bindTexture(gl.TEXTURE_2D, state.target);
        if (state.targetWidth !== width || state.targetHeight !== height) {
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
            state.targetWidth = width;
            state.targetHeight = height;
        }
        gl.bindTexture(gl.TEXTURE_2D, null);
        gl.bindFramebuffer(gl.FRAMEBUFFER, state.targetFramebuffer);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, state.target, 0);
    }

    // Reads one level (face, layer) of `texture`. `where`: {target (face or bind target), level, layer,
    // width, height (the expected level size; the readback is one pixel larger), fmt (formatInfo)}.
    // Returns {readback, error}. Pixels that the texture does not cover keep the sentinel.
    function read(gl, texture, where)
    {
        const width = where.width + 1;
        const height = where.height + 1;
        const readback = sentinelReadback(width, height);
        const fmt = where.fmt;
        const target = where.target;
        const bindTarget = bindTargetOf(target);
        const level = where.level || 0;
        const layer = where.layer || 0;
        takeError(gl);
        const savedUnpackBuffer = isWebGL2(gl) ? gl.getParameter(gl.PIXEL_UNPACK_BUFFER_BINDING) : null;
        const savedPackBuffer = isWebGL2(gl) ? gl.getParameter(gl.PIXEL_PACK_BUFFER_BINDING) : null;
        if (savedUnpackBuffer)
            gl.bindBuffer(gl.PIXEL_UNPACK_BUFFER, null);
        if (savedPackBuffer)
            gl.bindBuffer(gl.PIXEL_PACK_BUFFER, null);
        gl.pixelStorei(gl.PACK_ALIGNMENT, 4);
        const useFramebuffer = fmt.read === "fbo" && (isWebGL2(gl) || level === 0);
        if (useFramebuffer) {
            const state = stateOf(gl);
            // A level other than the base level can only be attached to a mipmap-complete texture.
            let savedBase = 0;
            let savedMax = 1000;
            const bt = enumValue(gl, bindTarget);
            if (isWebGL2(gl)) {
                gl.activeTexture(gl.TEXTURE1);
                gl.bindTexture(bt, texture);
                savedBase = gl.getTexParameter(bt, gl.TEXTURE_BASE_LEVEL);
                savedMax = gl.getTexParameter(bt, gl.TEXTURE_MAX_LEVEL);
                gl.texParameteri(bt, gl.TEXTURE_BASE_LEVEL, level);
                gl.texParameteri(bt, gl.TEXTURE_MAX_LEVEL, level);
            }
            gl.bindFramebuffer(gl.FRAMEBUFFER, state.framebuffer);
            if (isLayered(bindTarget))
                gl.framebufferTextureLayer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, texture, level, layer);
            else
                gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, enumValue(gl, target), texture, level);
            if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE)
                gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, readback.data);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, null, 0);
            gl.bindFramebuffer(gl.FRAMEBUFFER, null);
            if (isWebGL2(gl)) {
                gl.texParameteri(bt, gl.TEXTURE_BASE_LEVEL, savedBase);
                gl.texParameteri(bt, gl.TEXTURE_MAX_LEVEL, savedMax);
                gl.bindTexture(bt, null);
                gl.activeTexture(gl.TEXTURE0);
            }
        } else
            drawLevel(gl, texture, target, bindTarget, level, layer, where, width, height, readback);
        if (savedUnpackBuffer)
            gl.bindBuffer(gl.PIXEL_UNPACK_BUFFER, savedUnpackBuffer);
        if (savedPackBuffer)
            gl.bindBuffer(gl.PIXEL_PACK_BUFFER, savedPackBuffer);
        const error = takeError(gl);
        return { readback, error };
    }

    function drawLevel(gl, texture, target, bindTarget, level, layer, where, width, height, readback)
    {
        const fmt = where.fmt;
        const webgl2 = isWebGL2(gl);
        const kind = { "TEXTURE_2D": "2d", "TEXTURE_CUBE_MAP": "cube", "TEXTURE_2D_ARRAY": "2darray", "TEXTURE_3D": "3d" }[bindTarget];
        const program = webgl2 ? program2(gl, kind, fmt.read === "uint") : program1(gl, kind);
        const bt = enumValue(gl, bindTarget);
        const savedScissor = gl.isEnabled(gl.SCISSOR_TEST);
        const savedBlend = gl.isEnabled(gl.BLEND);
        gl.disable(gl.SCISSOR_TEST);
        gl.disable(gl.BLEND);
        gl.activeTexture(gl.TEXTURE0);
        const savedBinding0 = gl.getParameter(gl.TEXTURE_BINDING_2D);
        renderTarget(gl, width, height);
        gl.bindTexture(gl.TEXTURE_2D, savedBinding0);
        gl.activeTexture(gl.TEXTURE1);
        gl.bindTexture(bt, texture);
        const saved = {
            min: gl.getTexParameter(bt, gl.TEXTURE_MIN_FILTER),
            mag: gl.getTexParameter(bt, gl.TEXTURE_MAG_FILTER),
            wrapS: gl.getTexParameter(bt, gl.TEXTURE_WRAP_S),
            wrapT: gl.getTexParameter(bt, gl.TEXTURE_WRAP_T),
        };
        // WebGL 1 textures with non-power-of-two sizes are only complete with CLAMP_TO_EDGE.
        gl.texParameteri(bt, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(bt, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        if (webgl2) {
            saved.base = gl.getTexParameter(bt, gl.TEXTURE_BASE_LEVEL);
            saved.max = gl.getTexParameter(bt, gl.TEXTURE_MAX_LEVEL);
            if (isLayered(bindTarget) && where.lastLevel !== undefined) {
                // Layered textures with a complete mip chain are read with an explicit LOD: changing
                // TEXTURE_BASE_LEVEL of a TEXTURE_2D_ARRAY before drawing hits an ASSERT in ANGLE's
                // Metal backend (TextureMtl::onBaseMaxLevelsChanged) in debug builds.
                gl.texParameteri(bt, gl.TEXTURE_MAX_LEVEL, where.lastLevel);
                gl.texParameteri(bt, gl.TEXTURE_MIN_FILTER, gl.NEAREST_MIPMAP_NEAREST);
            } else {
                gl.texParameteri(bt, gl.TEXTURE_BASE_LEVEL, level);
                gl.texParameteri(bt, gl.TEXTURE_MAX_LEVEL, level);
                gl.texParameteri(bt, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
            }
        } else
            gl.texParameteri(bt, gl.TEXTURE_MIN_FILTER, level ? gl.NEAREST_MIPMAP_NEAREST : gl.NEAREST);
        gl.texParameteri(bt, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        const savedViewport = gl.getParameter(gl.VIEWPORT);
        const savedProgram = gl.getParameter(gl.CURRENT_PROGRAM);
        gl.viewport(0, 0, width, height);
        gl.useProgram(program);
        gl.uniform1i(gl.getUniformLocation(program, "t"), 1);
        const faceIndex = cubeFaces.indexOf(target);
        gl.uniform1i(gl.getUniformLocation(program, "face"), Math.max(0, faceIndex));
        gl.uniform1i(gl.getUniformLocation(program, "mode"), fmt.srgb ? 1 : 0);
        let savedVAO = null;
        let savedBuffer = null;
        if (webgl2) {
            gl.uniform1i(gl.getUniformLocation(program, "layer"), layer);
            gl.uniform1i(gl.getUniformLocation(program, "lod"), isLayered(bindTarget) && where.lastLevel !== undefined ? level : 0);
            gl.uniform1i(gl.getUniformLocation(program, "noAlpha"), fmt.channels.includes("a") ? 0 : 1);
            savedVAO = gl.getParameter(gl.VERTEX_ARRAY_BINDING);
            const state = stateOf(gl);
            if (!state.vao)
                state.vao = gl.createVertexArray();
            gl.bindVertexArray(state.vao);
            gl.drawArrays(gl.TRIANGLES, 0, 3);
            gl.bindVertexArray(savedVAO);
        } else {
            gl.uniform2f(gl.getUniformLocation(program, "size"), where.width, where.height);
            gl.uniform1f(gl.getUniformLocation(program, "level"), level);
            const state = stateOf(gl);
            if (!state.quad) {
                state.quad = gl.createBuffer();
                savedBuffer = gl.getParameter(gl.ARRAY_BUFFER_BINDING);
                gl.bindBuffer(gl.ARRAY_BUFFER, state.quad);
                gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
            } else {
                savedBuffer = gl.getParameter(gl.ARRAY_BUFFER_BINDING);
                gl.bindBuffer(gl.ARRAY_BUFFER, state.quad);
            }
            gl.enableVertexAttribArray(0);
            gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
            gl.drawArrays(gl.TRIANGLES, 0, 3);
            gl.bindBuffer(gl.ARRAY_BUFFER, savedBuffer);
        }
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, readback.data);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.useProgram(savedProgram);
        gl.viewport(...savedViewport);
        if (savedScissor)
            gl.enable(gl.SCISSOR_TEST);
        if (savedBlend)
            gl.enable(gl.BLEND);
        gl.texParameteri(bt, gl.TEXTURE_MIN_FILTER, saved.min);
        gl.texParameteri(bt, gl.TEXTURE_MAG_FILTER, saved.mag);
        gl.texParameteri(bt, gl.TEXTURE_WRAP_S, saved.wrapS);
        gl.texParameteri(bt, gl.TEXTURE_WRAP_T, saved.wrapT);
        // Layered textures read with an explicit LOD keep TEXTURE_MAX_LEVEL at their last level: setting
        // it back before the next draw hits the same ANGLE ASSERT.
        if (webgl2 && !(isLayered(bindTarget) && where.lastLevel !== undefined)) {
            gl.texParameteri(bt, gl.TEXTURE_BASE_LEVEL, saved.base);
            gl.texParameteri(bt, gl.TEXTURE_MAX_LEVEL, saved.max);
        }
        gl.bindTexture(bt, null);
        gl.activeTexture(gl.TEXTURE0);
    }

    // The observed size of a level in a readback from read(): the extent of the non-sentinel
    // pixels along row 0 and column 0.
    function observedSize(readback)
    {
        let w = 0;
        while (w < readback.width && !isSentinel(readback, w, 0))
            ++w;
        let h = 0;
        while (h < readback.height && !isSentinel(readback, 0, h))
            ++h;
        return { w, h };
    }

    // Checks that every pixel of the w x h level area outside the `exclude` rects is `color`
    // (within tolerance). Returns a failure string or null.
    function compareFill(readback, w, h, exclude, color, tolerance)
    {
        const width = Math.min(w, readback.width);
        const height = Math.min(h, readback.height);
        const words = new Uint32Array(readback.data.buffer, readback.data.byteOffset, readback.width * readback.height);
        const rounded = color.map(v => Math.max(0, Math.min(255, Math.round(v))));
        const exact = new Uint32Array(new Uint8Array(rounded).buffer)[0];
        let count = 0;
        let first = null;
        const checkPixel = (x, y) => {
            const index = y * readback.width + x;
            if (words[index] === exact)
                return;
            const i = index * 4;
            for (let k = 0; k < 4; ++k) {
                if (Math.abs(readback.data[i + k] - color[k]) > tolerance) {
                    if (!first)
                        first = `${x},${y} ${VideoFormats.formatValue(Array.from(readback.data.slice(i, i + 4)))}`;
                    ++count;
                    return;
                }
            }
        };
        for (let y = 0; y < height; ++y) {
            // Spans of this row outside the excluded rects.
            const cuts = exclude.filter(r => y >= r.y && y < r.y + r.h).map(r => [Math.max(0, r.x), Math.min(width, r.x + r.w)]).sort((a, b) => a[0] - b[0]);
            let x = 0;
            const rowBase = y * readback.width;
            for (const [start, end] of cuts.concat([[width, width]])) {
                for (; x < start; ++x) {
                    if (words[rowBase + x] !== exact)
                        checkPixel(x, y);
                }
                x = Math.max(x, end);
            }
        }
        if (!count)
            return null;
        return `${count} pixels differ from ${VideoFormats.formatValue(color)}, first at ${first}`;
    }

    // ---- Oracle ----

    const verticalFlip = "mirror+rotate180";

    // The source regions an upload copies, per written layer, in the coordinates of the oriented
    // image (after UNPACK_FLIP_Y_WEBGL, which flips the whole image before the sub-rectangle and
    // the slices are selected; UNPACK_ROW_LENGTH and UNPACK_ALIGNMENT do not apply to TexImageSource).
    // Returns [{layer, src, dest}].
    function uploadRegions(call, imageWidth, imageHeight)
    {
        const u = call.unpack || { };
        const sized = call.width !== undefined;
        const w = sized ? call.width : imageWidth;
        const h = sized ? call.height : imageHeight;
        const depth = call.depth || 1;
        const sliceHeight = u.imageHeight || h;
        const regions = [];
        for (let z = 0; z < depth; ++z) {
            regions.push({
                layer: (call.z || 0) + z,
                src: { x: sized ? (u.skipPixels || 0) : 0, y: (sized ? (u.skipRows || 0) : 0) + ((u.skipImages || 0) + z) * sliceHeight, w, h },
                dest: { x: call.x || 0, y: call.y || 0, w, h },
            });
        }
        return regions;
    }

    // Expected patches for one region, in level coordinates. image: {info, transform, frame}.
    function regionPatches(image, call, region, fmt)
    {
        const u = call.unpack || { };
        const transform = u.flipY ? VideoFormats.composeTransforms(image.transform, verticalFlip) : image.transform;
        // image.crop: the image shows only this rectangle of the (displayed) pattern, scaled to
        // image.width x image.height (VideoFrame visibleRect and displayWidth/displayHeight).
        let subRect = region.src;
        if (image.crop) {
            const c = image.crop;
            const sx = image.width / c.w;
            const sy = image.height / c.h;
            const layout = VideoFormats.layoutOf(image.info);
            const display = VideoFormats.transformedSize(image.transform, layout.width, layout.height);
            const cropY = u.flipY ? display.height - c.y - c.h : c.y;
            subRect = { x: c.x + region.src.x / sx, y: cropY + region.src.y / sy, w: region.src.w / sx, h: region.src.h / sy };
        }
        const patches = VideoFormats.expectedPatches(image.info, {
            colorSpace: "native",
            frame: image.frame || 0,
            transform,
            subRect,
            dest: region.dest,
            alpha: u.premultiply ? "premultiplied" : undefined,
        });
        return patchesForFormat(fmt, patches);
    }

    // ---- Upload path record (test hooks) ----

    function hasPathHooks()
    {
        return !!(window.internals && internals.webglLastVideoUpload && internals.setWebGLVideoUploadPath);
    }

    // The record of the last video upload that passed validation ({path, reason, pixelFormat, sequence}),
    // or null without the hooks or before the first upload.
    function lastUploadRecord(gl)
    {
        if (!window.internals || !internals.webglLastVideoUpload)
            return null;
        try {
            return internals.webglLastVideoUpload(gl) || null;
        } catch (e) {
            return null;
        }
    }

    // The record of the upload just made, or null without the hooks or when the upload did not get a
    // record (an error before either path was chosen). Each record is returned once.
    function uploadRecord(gl)
    {
        const record = lastUploadRecord(gl);
        if (!record)
            return null;
        const state = stateOf(gl);
        if (record.sequence === state.lastRecordSequence)
            return null;
        state.lastRecordSequence = record.sequence;
        return record;
    }

    // The forced upload paths that can be tested: only "default" without the hooks.
    function uploadPaths()
    {
        return hasPathHooks() ? ["default", "software", "hardware"] : ["default"];
    }

    function setUploadPath(gl, path)
    {
        if (!hasPathHooks()) {
            if (path !== "default")
                throw new Error("no internals.setWebGLVideoUploadPath");
            return;
        }
        internals.setWebGLVideoUploadPath(gl, path);
    }

    // "hardware (direct)" for a record.
    function recordDescription(record)
    {
        return record.reason ? `${record.path} (${record.reason})` : record.path;
    }

    // "default path: hardware (direct)" for the last upload, or null without the hooks.
    function pathDescription(gl)
    {
        const record = lastUploadRecord(gl);
        if (!record)
            return null;
        return `default path: ${recordDescription(record)}`;
    }

    return {
        name: "webgl",
        SENTINEL,
        enumValue,
        errorName,
        isWebGL2,
        createContext,
        releaseContext,
        releaseContexts,
        collectGarbage,
        disableConsoleErrors,
        takeError,
        webgl2Triples,
        webgl1Triples,
        triples,
        tripleLabel,
        sizedFormatOf,
        subImageCompatibility,
        formatInfo,
        storedValue,
        formatTolerance,
        patchesForFormat,
        fillData,
        resetUnpack,
        cubeFaces,
        bindTargetOf,
        isLayered,
        shortTarget,
        levelSize,
        faceTargets,
        defineDestination,
        upload,
        callLabel,
        read,
        observedSize,
        isSentinel,
        compareFill,
        uploadRegions,
        regionPatches,
        hasPathHooks,
        uploadRecord,
        lastUploadRecord,
        recordDescription,
        uploadPaths,
        setUploadPath,
        pathDescription,
    };
})();
