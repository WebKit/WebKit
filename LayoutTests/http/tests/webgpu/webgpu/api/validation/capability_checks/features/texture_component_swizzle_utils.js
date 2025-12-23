/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/






// Note: There are 4 settings with 6 options which is 1296 combinations. So we don't check them all. Just a few below.
export const kSwizzleTests = [
'rgba',
'0000',
'1111',
'rrrr',
'gggg',
'bbbb',
'aaaa',
'abgr',
'gbar',
'barg',
'argb',
'0gba',
'r0ba',
'rg0a',
'rgb0',
'1gba',
'r1ba',
'rg1a',
'rgb1'];



function swizzleComponentToTexelComponent(
src,
component)
{
  switch (component) {
    case '0':
      return 0;
    case '1':
      return 1;
    case 'r':
      return src.R;
    case 'g':
      return src.G;
    case 'b':
      return src.B;
    case 'a':
      return src.A;
  }
}

export function swizzleTexel(
src,
swizzle)
{
  return {
    R: swizzle[0] ?
    swizzleComponentToTexelComponent(src, swizzle[0]) :
    src.R,
    G: swizzle[1] ?
    swizzleComponentToTexelComponent(src, swizzle[1]) :
    src.G,
    B: swizzle[2] ?
    swizzleComponentToTexelComponent(src, swizzle[2]) :
    src.B,
    A: swizzle[3] ?
    swizzleComponentToTexelComponent(src, swizzle[3]) :
    src.A
  };
}

export function isIdentitySwizzle(swizzle) {
  return swizzle === 'rgba';
}