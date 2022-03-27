/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../common/util/util.js';
import { multiplyMatrices } from './math.js';

// These color space conversion function definitions are copied directly from
// CSS Color Module Level 4 Sample Code: https://drafts.csswg.org/css-color/#color-conversion-code

// Sample code for color conversions
// Conversion can also be done using ICC profiles and a Color Management System
// For clarity, a library is used for matrix multiplication (multiply-matrices.js)

// sRGB-related functions

/**
 * convert an array of sRGB values
 * where in-gamut values are in the range [0 - 1]
 * to linear light (un-companded) form.
 * https://en.wikipedia.org/wiki/SRGB
 * Extended transfer function:
 * for negative values,  linear portion is extended on reflection of axis,
 * then reflected power function is used.
 */
function lin_sRGB(RGB) {
  return RGB.map(val => {
    const sign = val < 0 ? -1 : 1;
    const abs = Math.abs(val);

    if (abs < 0.04045) {
      return val / 12.92;
    }

    return sign * Math.pow((abs + 0.055) / 1.055, 2.4);
  });
}

/**
 * convert an array of linear-light sRGB values in the range 0.0-1.0
 * to gamma corrected form
 * https://en.wikipedia.org/wiki/SRGB
 * Extended transfer function:
 * For negative values, linear portion extends on reflection
 * of axis, then uses reflected pow below that
 */
function gam_sRGB(RGB) {
  return RGB.map(val => {
    const sign = val < 0 ? -1 : 1;
    const abs = Math.abs(val);

    if (abs > 0.0031308) {
      return sign * (1.055 * Math.pow(abs, 1 / 2.4) - 0.055);
    }

    return 12.92 * val;
  });
}

/**
 * convert an array of linear-light sRGB values to CIE XYZ
 * using sRGB's own white, D65 (no chromatic adaptation)
 */
function lin_sRGB_to_XYZ(rgb) {
  const M = [
    [0.41239079926595934, 0.357584339383878, 0.1804807884018343],
    [0.21263900587151027, 0.715168678767756, 0.07219231536073371],
    [0.01933081871559182, 0.11919477979462598, 0.9505321522496607],
  ];

  return multiplyMatrices(M, rgb);
}

/**
 * convert XYZ to linear-light sRGB
 * using sRGB's own white, D65 (no chromatic adaptation)
 */
function XYZ_to_lin_sRGB(XYZ) {
  const M = [
    [3.2409699419045226, -1.537383177570094, -0.4986107602930034],
    [-0.9692436362808796, 1.8759675015077202, 0.04155505740717559],
    [0.05563007969699366, -0.20397695888897652, 1.0569715142428786],
  ];

  return multiplyMatrices(M, XYZ);
}

//  display-p3-related functions

/**
 * convert an array of display-p3 RGB values in the range 0.0 - 1.0
 * to linear light (un-companded) form.
 */
function lin_P3(RGB) {
  return lin_sRGB(RGB); // same as sRGB
}

/**
 * convert an array of linear-light display-p3 RGB  in the range 0.0-1.0
 * to gamma corrected form
 */
function gam_P3(RGB) {
  return gam_sRGB(RGB); // same as sRGB
}

/**
 * convert an array of linear-light display-p3 values to CIE XYZ
 * using display-p3's D65 (no chromatic adaptation)
 * http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
 */
function lin_P3_to_XYZ(rgb) {
  const M = [
    [0.4865709486482162, 0.26566769316909306, 0.1982172852343625],
    [0.2289745640697488, 0.6917385218365064, 0.079286914093745],
    [0.0, 0.04511338185890264, 1.043944368900976],
  ];

  // 0 was computed as -3.972075516933488e-17

  return multiplyMatrices(M, rgb);
}

/**
 * convert XYZ to linear-light P3
 * using display-p3's own white, D65 (no chromatic adaptation)
 */
function XYZ_to_lin_P3(XYZ) {
  const M = [
    [2.493496911941425, -0.9313836179191239, -0.40271078445071684],
    [-0.8294889695615747, 1.7626640603183463, 0.023624685841943577],
    [0.03584583024378447, -0.07617238926804182, 0.9568845240076872],
  ];

  return multiplyMatrices(M, XYZ);
}

/**
 * @returns the converted pixels in {R: number, G: number, B: number, A: number}.
 *
 * Follow conversion steps in CSS Color Module Level 4
 * https://drafts.csswg.org/css-color/#predefined-to-predefined
 * display-p3 and sRGB share the same white points.
 */
export function displayP3ToSrgb(pixel) {
  assert(
    pixel.R !== undefined && pixel.G !== undefined && pixel.B !== undefined,
    'color space conversion requires all of R, G and B components'
  );

  let rgbVec = [pixel.R, pixel.G, pixel.B];
  rgbVec = lin_P3(rgbVec);
  let rgbMatrix = [[rgbVec[0]], [rgbVec[1]], [rgbVec[2]]];
  rgbMatrix = XYZ_to_lin_sRGB(lin_P3_to_XYZ(rgbMatrix));
  rgbVec = [rgbMatrix[0][0], rgbMatrix[1][0], rgbMatrix[2][0]];
  rgbVec = gam_sRGB(rgbVec);

  pixel.R = rgbVec[0];
  pixel.G = rgbVec[1];
  pixel.B = rgbVec[2];

  return pixel;
}
/**
 * @returns the converted pixels in {R: number, G: number, B: number, A: number}.
 *
 * Follow conversion steps in CSS Color Module Level 4
 * https://drafts.csswg.org/css-color/#predefined-to-predefined
 * display-p3 and sRGB share the same white points.
 */
export function srgbToDisplayP3(pixel) {
  assert(
    pixel.R !== undefined && pixel.G !== undefined && pixel.B !== undefined,
    'color space conversion requires all of R, G and B components'
  );

  let rgbVec = [pixel.R, pixel.G, pixel.B];
  rgbVec = lin_sRGB(rgbVec);
  let rgbMatrix = [[rgbVec[0]], [rgbVec[1]], [rgbVec[2]]];
  rgbMatrix = XYZ_to_lin_P3(lin_sRGB_to_XYZ(rgbMatrix));
  rgbVec = [rgbMatrix[0][0], rgbMatrix[1][0], rgbMatrix[2][0]];
  rgbVec = gam_P3(rgbVec);

  pixel.R = rgbVec[0];
  pixel.G = rgbVec[1];
  pixel.B = rgbVec[2];

  return pixel;
}
