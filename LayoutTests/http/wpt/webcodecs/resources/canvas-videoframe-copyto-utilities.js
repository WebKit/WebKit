// Computes the values that VideoFrame.copyTo() and drawing a VideoFrame give for the frame of a canvas
// filled with one color, and reads them from a frame.

// The color is in the gamut of both sRGB and Display P3 when it is in any of the color spaces of the
// canvases, so that no conversion clips it.
const canvasFrameColor = [0.8, 0.5, 0.3];
// The values of the frames are 8-bit, and an unpremultiplied value is off by up to 255 / (2 * alpha)
// from its premultiplied value.
const canvasFrameTolerance = 2;
const canvasFrameContents = [
    { name: "opaque", alpha: 1 },
    { name: "translucent", alpha: 0.6 },
];
const canvasFrameCopyToFormats = [undefined, "RGBA", "BGRA"];
const canvasFrameDestinationColorSpaces = ["srgb", "display-p3"];

const xyzFromLinear = {
    "srgb": [
        [0.41239079926595934, 0.357584339383878, 0.1804807884018343],
        [0.21263900587151027, 0.715168678767756, 0.07219231536073371],
        [0.01933081871559182, 0.11919477979462598, 0.9505321522496607],
    ],
    "display-p3": [
        [0.4865709486482162, 0.26566769316909306, 0.1982172852343625],
        [0.2289745640697488, 0.6917385218365064, 0.079286914093745],
        [0, 0.04511338185890264, 1.043944368900976],
    ],
};
const linearFromXYZ = {
    "srgb": [
        [3.2409699419045226, -1.537383177570094, -0.4986107602930034],
        [-0.9692436362808796, 1.8759675015077202, 0.04155505740717559],
        [0.05563007969699366, -0.20397695888897652, 1.0569715142428786],
    ],
    "display-p3": [
        [2.4934969119414254, -0.9313836179191239, -0.40271078445071684],
        [-0.8294889695615747, 1.7626640603183463, 0.023624685841943577],
        [0.03584583024378447, -0.07617238926804182, 0.9568845240076872],
    ],
};

function srgbToLinear(value)
{
    return value <= 0.04045 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4);
}

function linearToSRGB(value)
{
    return value <= 0.0031308 ? value * 12.92 : 1.055 * Math.pow(value, 1 / 2.4) - 0.055;
}

function multiply(matrix, vector)
{
    return matrix.map(row => row[0] * vector[0] + row[1] * vector[1] + row[2] * vector[2]);
}

// Converts a color of a canvas color space, such as "display-p3-linear", to "srgb" or "display-p3".
function convertColor(rgb, sourceColorSpace, destinationColorSpace)
{
    const isLinear = sourceColorSpace.endsWith("-linear");
    const sourcePrimaries = sourceColorSpace.startsWith("display-p3") ? "display-p3" : "srgb";
    let linear = isLinear ? rgb : rgb.map(srgbToLinear);
    if (sourcePrimaries != destinationColorSpace)
        linear = multiply(linearFromXYZ[destinationColorSpace], multiply(xyzFromLinear[sourcePrimaries], linear));
    return linear.map(linearToSRGB);
}

function to8Bit(value)
{
    return Math.round(Math.min(Math.max(value, 0), 1) * 255);
}

// The values the canvas is filled with: canvasFrameColor with the alpha of the content,
// premultiplied when the canvas stores premultiplied colors.
function canvasFillColor(content, isPremultiplied)
{
    const color = canvasFrameColor.map(value => isPremultiplied ? value * content.alpha : value);
    return [...color, content.alpha];
}

// Returns the straight alpha 8-bit RGBA values of the frame of a canvas filled by canvasFillColor(), in
// destinationColorSpace. The colors of an opaque canvas are what it stores, as its alpha is ignored.
function expectedCanvasFramePixel(canvas, content, destinationColorSpace)
{
    const alpha = to8Bit(content.alpha);
    const stored = canvasFillColor(content, canvas.isPremultiplied).slice(0, 3).map(to8Bit);
    let rgb;
    if (canvas.isOpaque || !canvas.isPremultiplied)
        rgb = stored.map(value => value / 255);
    else
        rgb = stored.map(value => Math.min(Math.round(value * 255 / alpha), 255) / 255);
    const converted = convertColor(rgb, canvas.colorSpace, destinationColorSpace).map(to8Bit);
    return [...converted, canvas.isOpaque ? 255 : alpha];
}

// Reads the pixel at (1, 1) with copyTo(), as RGBA.
async function copyToPixel(frame, format, colorSpace)
{
    const options = { colorSpace };
    if (format)
        options.format = format;
    const buffer = new Uint8Array(frame.allocationSize(options));
    const layout = await frame.copyTo(buffer, options);
    const offset = layout[0].offset + layout[0].stride + 4;
    const pixel = Array.from(buffer.subarray(offset, offset + 4));
    if ((format ?? frame.format).startsWith("BGR"))
        return [pixel[2], pixel[1], pixel[0], pixel[3]];
    return pixel;
}

// Reads the pixel at (1, 1) of the frame drawn into a 2D canvas of the color space.
function drawnPixel(frame, colorSpace)
{
    const canvas = new OffscreenCanvas(frame.displayWidth, frame.displayHeight);
    const context = canvas.getContext("2d", { colorSpace });
    context.drawImage(frame, 0, 0);
    return Array.from(context.getImageData(1, 1, 1, 1, { colorSpace }).data);
}

function pixelMatches(actual, expected)
{
    return actual.length == expected.length && actual.every((value, i) => Math.abs(value - expected[i]) <= canvasFrameTolerance);
}

// Returns the descriptions of the reads of the frame that do not give the expected values.
async function canvasFrameMismatches(frame, canvas, content)
{
    const mismatches = [];
    for (const colorSpace of canvasFrameDestinationColorSpaces) {
        const expected = expectedCanvasFramePixel(canvas, content, colorSpace);
        for (const format of canvasFrameCopyToFormats) {
            const actual = await copyToPixel(frame, format, colorSpace);
            if (!pixelMatches(actual, expected))
                mismatches.push(`copyTo({ format: ${format ?? "default"}, colorSpace: ${colorSpace} }) gave ${actual}, expected ${expected}`);
        }
        const actual = drawnPixel(frame, colorSpace);
        if (!pixelMatches(actual, expected))
            mismatches.push(`drawImage() to a ${colorSpace} canvas gave ${actual}, expected ${expected}`);
    }
    return mismatches;
}
