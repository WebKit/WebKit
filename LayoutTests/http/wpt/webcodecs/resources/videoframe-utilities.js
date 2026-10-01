function sRGBToBT709Gamma(value) {
    // sRGB -> linear. https://en.wikipedia.org/wiki/SRGB#Transfer_function_(%22gamma%22)
    const normalized = value / 255.0;
    const linear = normalized <= 0.04045 ? normalized / 12.92 : Math.pow((normalized + 0.055) / 1.055, 2.4);
    // linear -> BT709. https://en.wikipedia.org/wiki/Rec._709#Non-linear_encoding
    if (linear < 0.018)
        return 4.5 * linear * 255.0;
    return (1.099 * Math.pow(linear, 0.45) - 0.099) * 255.0;
}

// Luma coefficients Kr and Kb of the VideoColorSpace matrices. Kg is 1 - Kr - Kb.
// https://en.wikipedia.org/wiki/YCbCr#YCbCr
const matrixLumaCoefficients = {
    "bt709": { kr: 0.2126, kb: 0.0722 },
    "bt470bg": { kr: 0.299, kb: 0.114 },
    "smpte170m": { kr: 0.299, kb: 0.114 },
    "smpte240m": { kr: 0.212, kb: 0.087 },
    "bt2020-ncl": { kr: 0.2627, kb: 0.0593 },
};

function lumaCoefficients(matrix) {
    const coefficients = matrixLumaCoefficients[matrix];
    if (!coefficients)
        throw new Error(`Unsupported matrix: ${matrix}`);
    return { ...coefficients, kg: 1 - coefficients.kr - coefficients.kb };
}

// Converts an sRGB component to the given transfer function. Supports "bt709" and "iec61966-2-1" (sRGB).
function sRGBToTransferGamma(value, transfer) {
    if (transfer === "iec61966-2-1")
        return value;
    if (transfer === "bt709")
        return sRGBToBT709Gamma(value);
    throw new Error(`Unsupported transfer: ${transfer}`);
}

function transferToSRGBGamma(value, transfer) {
    if (transfer === "iec61966-2-1")
        return value;
    if (transfer === "bt709")
        return BT709ToSRGBGamma(value);
    throw new Error(`Unsupported transfer: ${transfer}`);
}

// Converts source sRGB ImageData to I420 data in the given VideoColorSpaceInit.
// Only BT709 primaries are supported, as sRGB and BT709 have the same primaries.
function sRGBImageDataToI420(imageData, colorSpace) {
    if (colorSpace.primaries !== "bt709")
        throw new Error(`Unsupported primaries: ${colorSpace.primaries}`);
    const { kr, kg, kb } = lumaCoefficients(colorSpace.matrix);
    const fullRange = colorSpace.fullRange;
    const width = imageData.width;
    const height = imageData.height;
    const rgba = imageData.data;

    const chromaWidth = Math.ceil(width / 2);
    const chromaHeight = Math.ceil(height / 2);

    const ySize = width * height;
    const uvSize = chromaWidth * chromaHeight;
    const totalSize = ySize + 2 * uvSize;

    const data = new Uint8Array(totalSize);
    const yPlane = new Uint8ClampedArray(data.buffer, 0, ySize);
    const uPlane = new Uint8ClampedArray(data.buffer, ySize, uvSize);
    const vPlane = new Uint8ClampedArray(data.buffer, ySize + uvSize, uvSize);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const i = (y * width + x) * 4;
            const r = sRGBToTransferGamma(rgba[i + 0], colorSpace.transfer);
            const g = sRGBToTransferGamma(rgba[i + 1], colorSpace.transfer);
            const b = sRGBToTransferGamma(rgba[i + 2], colorSpace.transfer);
            // https://en.wikipedia.org/wiki/YCbCr#YCbCr
            const yIndex =  y * width + x;
            const luma = kr * r + kg * g + kb * b;
            let Y = luma;
            if (!fullRange)
                Y = 16 + 219 * Y / 255;
            yPlane[yIndex] = Math.round(Y);
            if (y % 2 === 0 && x % 2 === 0) {
                const uvIndex = (y / 2) * chromaWidth + (x / 2);
                let U = (b - luma) / (2 * (1 - kb)) + 128;
                let V = (r - luma) / (2 * (1 - kr)) + 128;
                if (!fullRange) {
                    U = 224 * (U - 128) / 255 + 128;
                    V = 224 * (V - 128) / 255 + 128;
                }
                uPlane[uvIndex] = Math.round(U);
                vPlane[uvIndex] = Math.round(V);
            }
        }
    }
    return {
        data,
        width,
        height,
        chromaWidth,
        chromaHeight,
        format: "I420",
        colorSpace: { ...colorSpace },
        layout: [
            { offset: 0, stride: width },
            { offset: ySize, stride: chromaWidth },
            { offset: ySize + uvSize, stride: chromaWidth }
        ]
    };
}

// Converts source sRGB ImageData to I420 BT709 data.
function sRGBImageDataToI420BT709(imageData, fullRange) {
    return sRGBImageDataToI420(imageData, { primaries: "bt709", transfer: "bt709", matrix: "bt709", fullRange });
}

function BT709ToSRGBGamma(value) {
    // BT.709 -> linear. https://en.wikipedia.org/wiki/Rec._709#Non-linear_decoding
    const normalized = value / 255.0;
    const linear = normalized < 0.081 ? normalized / 4.5 : Math.pow((normalized + 0.099) / 1.099, 1 / 0.45);

    // linear -> sRGB. https://en.wikipedia.org/wiki/SRGB#Transfer_function_(%22gamma%22)
    if (linear <= 0.0031308)
        return 12.92 * linear * 255.0;
    return (1.055 * Math.pow(linear, 1 / 2.4) - 0.055) * 255.0;
}

// Converts I420 YUV data, in the VideoColorSpaceInit of i420Data.colorSpace, to sRGB RGBA format.
function I420ToSRGB(i420Data) {
    const colorSpace = i420Data.colorSpace;
    if (colorSpace.primaries !== "bt709")
        throw new Error(`Unsupported primaries: ${colorSpace.primaries}`);
    const { kr, kg, kb } = lumaCoefficients(colorSpace.matrix);
    const fullRange = colorSpace.fullRange;
    const width = i420Data.width;
    const height = i420Data.height;
    const chromaWidth = i420Data.chromaWidth;
    const chromaHeight = i420Data.chromaHeight;

    const ySize = width * height;
    const uvSize = chromaWidth * chromaHeight;

    const yPlane = new Uint8Array(i420Data.data.buffer, 0, ySize);
    const uPlane = new Uint8Array(i420Data.data.buffer, ySize, uvSize);
    const vPlane = new Uint8Array(i420Data.data.buffer, ySize + uvSize, uvSize);

    const data = new Uint8ClampedArray(width * height * 4);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            // https://en.wikipedia.org/wiki/YCbCr#YCbCr.
            // Solve for r, g, b to get the values based on Y, Cb, Cr.
            const yIndex = y * width + x;
            let Y = yPlane[yIndex];
            const uvIndex = Math.floor(y / 2) * chromaWidth + Math.floor(x / 2);
            let Cb = uPlane[uvIndex] - 128;
            let Cr = vPlane[uvIndex] - 128;
            if (!fullRange) {
                Y = (Y - 16) * 255 / 219;
                Cb = Cb * 255 / 224;
                Cr = Cr * 255 / 224;
            }
            const r = Y + 2 * (1 - kr) * Cr;
            const g = Y - 2 * (1 - kb) * kb / kg * Cb - 2 * (1 - kr) * kr / kg * Cr;
            const b = Y + 2 * (1 - kb) * Cb;

            const i = (y * width + x) * 4;
            data[i + 0] = transferToSRGBGamma(r, colorSpace.transfer);
            data[i + 1] = transferToSRGBGamma(g, colorSpace.transfer);
            data[i + 2] = transferToSRGBGamma(b, colorSpace.transfer);
            data[i + 3] = 255;
        }
    }
    return {
        data,
        width,
        height,
        format: "RGBA",
        colorSpace: {
            primaries: "bt709",
            transfer: "iec61966-2-1",
            matrix: "rgb",
            fullRange: true
        },

        layout: [{ offset: 0, stride: width * 4 }]
    };
}

// Converts I420 BT.709 YUV data to sRGB RGBA format.
function I420BT709ToSRGB(i420Data, fullRange) {
    return I420ToSRGB({ ...i420Data, colorSpace: { primaries: "bt709", transfer: "bt709", matrix: "bt709", fullRange } });
}

// Creates a VideoFrame of the data returned by sRGBImageDataToI420() or I420ToSRGB().
function createVideoFrame(frameData) {
    return new VideoFrame(frameData.data, {
        format: frameData.format,
        colorSpace: frameData.colorSpace,
        codedWidth: frameData.width,
        codedHeight: frameData.height,
        layout: frameData.layout,
        timestamp: 0
    });
}

function createTestImageData(width, height, color) {
    const data = new Uint8Array(width * height * 4);
    for (let y = 0; y < height; ++y) {
        for (let x = 0; x < width; ++x) {
            const o = (y * width + x) * 4;
            for (let i = 0; i < 4; ++i)
                data[o + i] = color[i];
        }
    }
    return { width, height, data };
}

// Recreates the video frame through a video codec.
async function encodeDecodeVideoFrame(videoFrame, codec, preferSoftware) {
    codec = codec || "vp09.00.10.08";
    let newFrame;
    let decoder = new VideoDecoder({
        output: (frame) => {
            newFrame = frame;
        },
        error: (e) => {
            throw e;
        }
    });
    const hardwareAcceleration = preferSoftware ? "prefer-software" : "no-preference";
    decoder.configure({
        codec: codec,
        hardwareAcceleration
    });
    encoder = new VideoEncoder({
        output: (chunk) => {
            decoder.decode(chunk);
        },
        error: (e) => {
            throw e;
        }
    });
    encoder.configure({
        codec: codec,
        width: videoFrame.codedWidth,
        height: videoFrame.codedHeight,
        bitrate: 10e6,
        framerate: 30,
        hardwareAcceleration
    });
    encoder.encode(videoFrame, {
        keyFrame: true
    });
    await encoder.flush();
    await decoder.flush();
    return newFrame;
}
