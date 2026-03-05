function sRGBToBT709Gamma(value) {
    // sRGB -> linear. https://en.wikipedia.org/wiki/SRGB#Transfer_function_(%22gamma%22)
    const normalized = value / 255.0;
    const linear = normalized <= 0.04045 ? normalized / 12.92 : Math.pow((normalized + 0.055) / 1.055, 2.4);
    // linear -> BT709. https://en.wikipedia.org/wiki/Rec._709#Non-linear_encoding
    if (linear < 0.018)
        return 4.5 * linear * 255.0;
    return (1.099 * Math.pow(linear, 0.45) - 0.099) * 255.0;
}

// Converts source sRGB ImageData to I420 BT709 data.
// sRGB and BT709 have the same primaries but different transfer functions.
function sRGBImageDataToI420BT709(imageData, fullRange) {
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
            const r = sRGBToBT709Gamma(rgba[i + 0]);
            const g = sRGBToBT709Gamma(rgba[i + 1]);
            const b = sRGBToBT709Gamma(rgba[i + 2]);
            // https://en.wikipedia.org/wiki/Rec._709#The_Y'C'BC'R_color_space
            const yIndex =  y * width + x;
            let Y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            if (!fullRange)
                Y = 16 + 219 * Y / 255;
            yPlane[yIndex] = Math.round(Y);
            if (y % 2 === 0 && x % 2 === 0) {
                const uvIndex = (y / 2) * chromaWidth + (x / 2);
                let U = -0.114572 * r - 0.385428 * g + 0.5 * b + 128;
                let V = 0.5 * r - 0.454153 * g - 0.045847 * b + 128;
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
        colorSpace: {
            primaries: "bt709",
            transfer: "bt709",
            matrix: "bt709",
            fullRange
        },
        layout: [
            { offset: 0, stride: width },
            { offset: ySize, stride: chromaWidth },
            { offset: ySize + uvSize, stride: chromaWidth }
        ]
    };
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

// Converts I420 BT.709 YUV data to sRGB RGBA format.
function I420BT709ToSRGB(i420Data, fullRange) {
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
            // https://en.wikipedia.org/wiki/Rec._709#The_Y'C'BC'R_color_space.
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
            const r = Y + 1.5748 * Cr;
            const g = Y - 0.1873 * Cb - 0.4681 * Cr;
            const b = Y + 1.8556 * Cb;

            const i = (y * width + x) * 4;
            data[i + 0] = BT709ToSRGBGamma(r);
            data[i + 1] = BT709ToSRGBGamma(g);
            data[i + 2] = BT709ToSRGBGamma(b);
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
async function encodeDecodeVideoFrame(videoFrame, codec) {
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
    decoder.configure({
        codec: codec,
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
        framerate: 30
    });
    encoder.encode(videoFrame, {
        keyFrame: true
    });
    await encoder.flush();
    await decoder.flush();
    return newFrame;
}