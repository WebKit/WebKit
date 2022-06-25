/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ /**
 * Reifies a `GPUOrigin3D` into a `Required<GPUOrigin3DDict>`.
 */ export function reifyOrigin3D(val) {
  if (Symbol.iterator in val) {
    const v = Array.from(val);
    return {
      x: v[0] ?? 0,
      y: v[1] ?? 0,
      z: v[2] ?? 0,
    };
  } else {
    const v = val;
    return {
      x: v.x ?? 0,
      y: v.y ?? 0,
      z: v.z ?? 0,
    };
  }
}

/**
 * Reifies a `GPUExtent3D` into a `Required<GPUExtent3DDict>`.
 */
export function reifyExtent3D(val) {
  if (Symbol.iterator in val) {
    const v = Array.from(val);
    return {
      width: v[0] ?? 1,
      height: v[1] ?? 1,
      depthOrArrayLayers: v[2] ?? 1,
    };
  } else {
    const v = val;
    return {
      width: v.width ?? 1,
      height: v.height ?? 1,
      depthOrArrayLayers: v.depthOrArrayLayers ?? 1,
    };
  }
}
