description("Tests that put/getImageData work with float16 canvas.");

debug(`canvas.width: ${canvas.width}`)
debug(`canvas.height: ${canvas.height}`)
debug(`canvas.dataset.colorSpace: ${canvas.dataset.colorSpace}`)

const context = canvas.getContext("2d", { colorSpace: canvas.dataset.colorSpace, colorType: "float16" });
shouldBe('context.getContextAttributes().colorSpace', 'canvas.dataset.colorSpace');
shouldBe('context.getContextAttributes().colorType', '"float16"');

if (typeof context.getEffectiveRenderingModeForTesting !== "undefined") {
    debug(`Effective renderingMode: ${context.getEffectiveRenderingModeForTesting()}`);
}

const f_r = 0;
const f_g = 0.5;
const f_b = 1;
const f_a = 1;
// FIXME: Fill still only works in [0-1], adapt when extended/float16 >1 support is available.
context.fillStyle = `color(${canvas.dataset.colorSpace} ${f_r} ${f_g} ${f_b})`;
context.fillRect(0, 0, canvas.width, canvas.height);

const componentsPerPixel = 4;

function shouldBeAround(to_eval, targetNumber, tolerance)
{
    if (!tolerance)
        return shouldBe(to_eval, String(targetNumber));
    return shouldBeCloseTo(to_eval, targetNumber, tolerance);
}

function verifyImageData(variable, constructor, bytesPerElement, red, green, blue, alpha, tolerance)
{
    shouldBe(variable + '.width', '1');
    shouldBe(variable + '.height', '1');
    shouldBe(variable + '.data.constructor', constructor);
    shouldBe(variable + '.data.BYTES_PER_ELEMENT', String(bytesPerElement));
    shouldBe(variable + '.data.length', '4');
    shouldBe(variable + '.data.byteLength', String(bytesPerElement * 4));
    shouldBeAround(variable + '.data.at(0)', red, tolerance);
    shouldBeAround(variable + '.data.at(1)', green, tolerance);
    shouldBeAround(variable + '.data.at(2)', blue, tolerance);
    shouldBeAround(variable + '.data.at(3)', alpha, tolerance);
}

function areEqualImageData(imageDataActual, imageDataExpected, tolerance)
{
    tolerance = 0;
    shouldBe(imageDataActual + '.width', imageDataExpected + '.width');
    shouldBe(imageDataActual + '.height', imageDataExpected + '.height');
    shouldBe(imageDataActual + '.data.constructor', imageDataExpected + '.data.constructor');
    shouldBe(imageDataActual + '.data.BYTES_PER_ELEMENT', imageDataExpected + '.data.BYTES_PER_ELEMENT');
    shouldBe(imageDataActual + '.data.length', imageDataExpected + '.data.length');
    shouldBe(imageDataActual + '.data.byteLength', imageDataExpected + '.data.byteLength');
    const actualData = eval(imageDataActual).data;
    const expectedData = eval(imageDataExpected).data;
    for (component = 0; component < actualData.length; ++component) {
        if (Math.abs(actualData[component] - expectedData[component]) > tolerance)
            shouldBeAround(imageDataActual + '.data[' + component + ']', expectedData[component], tolerance, true);
    }
}

const uint8_bytes_per_element = 1;
const float16_bytes_per_element = 2;
// Tolerance: Half of the uint8 color component unit.
const uint8_nonzero_tolerance = 0.5;
const float16_nonzero_tolerance = uint8_nonzero_tolerance / 255;

var created_imageData_float16 = context.createImageData(1, 1, { pixelFormat: "rgba-float16" });
verifyImageData('created_imageData_float16', 'Float16Array', float16_bytes_per_element, 0, 0, 0, 0, 0);

var gotten_imageData_float16 = context.getImageData(0, 0, 1, 1, { pixelFormat: "rgba-float16" });
verifyImageData('gotten_imageData_float16', 'Float16Array', float16_bytes_per_element, f_r, f_g, f_b, f_a, float16_nonzero_tolerance);

var gotten_imageData_float16_last = context.getImageData(canvas.width - 1, canvas.height - 2, 1, 1, { pixelFormat: "rgba-float16" });
verifyImageData('gotten_imageData_float16_last', 'Float16Array', float16_bytes_per_element, f_r, f_g, f_b, f_a, float16_nonzero_tolerance);

var created_imageData_uint8 = context.createImageData(1, 1, { pixelFormat: "rgba-unorm8" });
verifyImageData('created_imageData_uint8', 'Uint8ClampedArray', uint8_bytes_per_element, 0, 0, 0, 0);

// This verifies the basic float16->uint8 conversion:
var gotten_imageData_uint8 = context.getImageData(0, 0, 1, 1, { pixelFormat: "rgba-unorm8" });
verifyImageData('gotten_imageData_uint8', 'Uint8ClampedArray', uint8_bytes_per_element, f_r * 255, f_g * 255, f_b * 255, f_a * 255, uint8_nonzero_tolerance);

var gotten_imageData_uint8_last = context.getImageData(canvas.width - 1, canvas.height - 2, 1, 1, { pixelFormat: "rgba-unorm8" });
verifyImageData('gotten_imageData_uint8_last', 'Uint8ClampedArray', uint8_bytes_per_element, f_r * 255, f_g * 255, f_b * 255, f_a * 255, uint8_nonzero_tolerance);

// Put the float16 ImageData back into the (uint8-backed) canvas, and get a default (uint8) ImageData.
// This verifies the basic uint8->float16 conversion.
context.clearRect(0, 0, 1, 1);
context.putImageData(gotten_imageData_float16, 0, 0);
var gotten_imageData_uint8_from_float16 = context.getImageData(0, 0, 1, 1);
verifyImageData('gotten_imageData_uint8_from_float16', 'Uint8ClampedArray', uint8_bytes_per_element, f_r * 255, f_g * 255, f_b * 255, f_a * 255, uint8_nonzero_tolerance);

// Exhaustive uint8->float16->uint8 round-trip test for all possible individual uint8 component values. Only log errors.
var componentsToTest = componentsPerPixel;
shouldBeTrue('canvas.width * canvas.height >= 256 * componentsToTest');

var input_imageData_uint8 = context.createImageData(canvas.width, canvas.height);
var expected_imageData_uint8 = context.createImageData(canvas.width, canvas.height);
var expected_imageData_float16 = context.createImageData(canvas.width, canvas.height, { pixelFormat: "rgba-float16" });

var pixelOffset = 0;
for (let v = 0; v < 256; ++v) {
    for (component = 0; component < componentsToTest; ++component) {
        let r = 0;
        let g = 0;
        let b = 0;
        let a = 255;
        switch (component) {
        case 0: r = v; break;
        case 1: g = v; break;
        case 2: b = v; break;
        case 3: r = g = b = (v ? 255 : 0); a = v; break;
        }
        input_imageData_uint8.data[pixelOffset * componentsPerPixel + 0] = r;
        input_imageData_uint8.data[pixelOffset * componentsPerPixel + 1] = g;
        input_imageData_uint8.data[pixelOffset * componentsPerPixel + 2] = b;
        input_imageData_uint8.data[pixelOffset * componentsPerPixel + 3] = a;

        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 0] = r;
        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 1] = g;
        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 2] = b;
        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 3] = a;

        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 0] = r / 255;
        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 1] = g / 255;
        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 2] = b / 255;
        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 3] = a / 255;

        ++pixelOffset;
    }
}
shouldBe('pixelOffset', '256 * componentsToTest');

context.putImageData(input_imageData_uint8, 0, 0);
gotten_imageData_uint8 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-unorm8" });
areEqualImageData('gotten_imageData_uint8', 'expected_imageData_uint8', 0);
gotten_imageData_float16 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-float16" });
areEqualImageData('gotten_imageData_float16', 'expected_imageData_float16', float16_nonzero_tolerance);

context.clearRect(0, 0, canvas.width, canvas.height);
context.putImageData(gotten_imageData_float16, 0, 0);
gotten_imageData_float16 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-float16" });
areEqualImageData('gotten_imageData_float16', 'expected_imageData_float16', float16_nonzero_tolerance);
gotten_imageData_uint8 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-unorm8" });
areEqualImageData('gotten_imageData_uint8', 'expected_imageData_uint8', 0);

// Deeper float16->(same)float16->uint8->(nearby)float16 round-trip test for many possible individual component values. Only log errors.
const divisions = 1024;
componentsToTest = 3;
const maxValue = 2;
// A few extras:      largest subnormal    smallest normal  largest <1     smallest >1  largest ...
const extraValues = [ 6.097555160522461e-5, 6.103515625e-5, 0.99951171875, 1.0009765625, 65504, -1, -65504, Infinity, -Infinity ];
shouldBeTrue('canvas.width * canvas.height >= (extraValues.length + divisions) * componentsToTest');

input_imageData_float16 = context.createImageData(canvas.width, canvas.height, { pixelFormat: "rgba-float16" });
expected_imageData_uint8 = context.createImageData(canvas.width, canvas.height);
expected_imageData_float16 = context.createImageData(canvas.width, canvas.height, { pixelFormat: "rgba-float16" });

pixelOffset = 0;
const clamp01 = (x) => Math.min(Math.max(x, 0), 1);
for (let division = -extraValues.length; division < divisions; ++division) {
    const floatValue = (division >= 0) ? (maxValue * division / divisions) : extraValues[extraValues.length + division];
    for (component = 0; component < componentsToTest; ++component) {
        let r = 0;
        let g = 0;
        let b = 0;
        let a = 1;
        switch (component) {
        case 0: r = floatValue; break;
        case 1: g = floatValue; break;
        case 2: b = floatValue; break;
        }
        input_imageData_float16.data[pixelOffset * componentsPerPixel + 0] = r;
        input_imageData_float16.data[pixelOffset * componentsPerPixel + 1] = g;
        input_imageData_float16.data[pixelOffset * componentsPerPixel + 2] = b;
        input_imageData_float16.data[pixelOffset * componentsPerPixel + 3] = a;

        // Convert float16 [0,1] to expected [0,255] value, with a tolerance of half a unit.
        r = clamp01(r) * 255;
        g = clamp01(g) * 255;
        b = clamp01(b) * 255;
        a = clamp01(a) * 255;
        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 0] = r;
        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 1] = g;
        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 2] = b;
        expected_imageData_uint8.data[pixelOffset * componentsPerPixel + 3] = a;

        r = Math.round(r) / 255;
        g = Math.round(g) / 255;
        b = Math.round(b) / 255;
        a = Math.round(a) / 255;
        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 0] = r;
        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 1] = g;
        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 2] = b;
        expected_imageData_float16.data[pixelOffset * componentsPerPixel + 3] = a;

        ++pixelOffset;
    }
}
shouldBe('pixelOffset', '(extraValues.length + divisions) * componentsToTest');

context.putImageData(input_imageData_float16, 0, 0);

gotten_imageData_float16 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-float16" });
areEqualImageData('gotten_imageData_float16', 'input_imageData_float16', float16_nonzero_tolerance);

gotten_imageData_uint8 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-unorm8" });
areEqualImageData('gotten_imageData_uint8', 'expected_imageData_uint8', 0);

context.putImageData(gotten_imageData_uint8, 0, 0);
gotten_imageData_uint8_from_float16 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-unorm8" });
areEqualImageData('gotten_imageData_uint8_from_float16', 'expected_imageData_uint8', 0);
gotten_imageData_float16 = context.getImageData(0, 0, canvas.width, canvas.height, { pixelFormat: "rgba-float16" });
areEqualImageData('gotten_imageData_float16', 'expected_imageData_float16', float16_nonzero_tolerance);

shouldThrowErrorName(`context.createImageData(1, 1, { pixelFormat: "foo" })`, "TypeError")
shouldThrowErrorName(`context.getImageData(0, 0, 1, 1, { pixelFormat: "foo" })`, "TypeError")
shouldThrowErrorName(`new ImageData(new Uint8ClampedArray(4), 1, 1, { colorSpace: "${canvas.dataset.colorSpace}", pixelFormat: "rgba-float16" })`, "InvalidStateError")
shouldThrowErrorName(`new ImageData(new Float16Array(4), 1, 1, { colorSpace: "${canvas.dataset.colorSpace}", pixelFormat: "rgba-unorm8" })`, "InvalidStateError")
