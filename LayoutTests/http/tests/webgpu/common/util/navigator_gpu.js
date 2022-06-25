/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/ /// <reference types="@webgpu/types" />
import { assert } from './util.js';
/**
 * Finds and returns the `navigator.gpu` object (or equivalent, for non-browser implementations).
 * Throws an exception if not found.
 */
function defaultGPUProvider() {
  assert(
  typeof navigator !== 'undefined' && navigator.gpu !== undefined,
  'No WebGPU implementation found');

  return navigator.gpu;
}

/**
 * GPUProvider is a function that creates and returns a new GPU instance.
 * May throw an exception if a GPU cannot be created.
 */


let gpuProvider = defaultGPUProvider;

/**
 * Sets the function to create and return a new GPU instance.
 */
export function setGPUProvider(provider) {
  assert(impl === undefined, 'setGPUProvider() should not be after getGPU()');
  gpuProvider = provider;
}

let impl = undefined;

/**
 * Finds and returns the `navigator.gpu` object (or equivalent, for non-browser implementations).
 * Throws an exception if not found.
 */
export function getGPU() {
  if (impl) {
    return impl;
  }

  impl = gpuProvider();

  return impl;
}
//# sourceMappingURL=navigator_gpu.js.map