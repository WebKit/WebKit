/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const kRenderEncodeTypes = ['render pass', 'render bundle'];

export const kProgrammableEncoderTypes = ['compute pass', ...kRenderEncodeTypes];

export const kEncoderTypes = ['non-pass', ...kProgrammableEncoderTypes];

/** See {@link webgpu/api/validation/validation_test.ValidationTest.createEncoder |
 * GPUTest.createEncoder()}. */
export class CommandBufferMaker {
  /** `GPU___Encoder` for recording commands into. */
  // Look up the type of the encoder based on `T`. If `T` is a union, this will be too!

  /**
   * Finish any passes, finish and record any bundles, and finish/return the command buffer.
   * Checks for validation errors in (only) the appropriate finish call.
   */

  /**
   * Finish the command buffer and submit it. Checks for validation errors in either the submit or
   * the appropriate finish call, depending on the state of a resource used in the encoding.
   */

  /**
   * `validateFinishAndSubmit()` based on the state of a resource in the command encoder.
   * - `finish()` should fail if the resource is 'invalid'.
   * - Only `submit()` should fail if the resource is 'destroyed'.
   */

  constructor(t, encoder, finish) {
    // TypeScript introduces an intersection type here where we don't want one.
    this.encoder = encoder;
    this.validateFinish = finish;

    // Define extra methods like this, otherwise they get unbound when destructured, e.g.:
    // const { encoder, validateFinishAndSubmit } = t.createEncoder(type);

    this.validateFinishAndSubmit = (shouldBeValid, submitShouldSucceedIfValid) => {
      const commandBuffer = finish(shouldBeValid);
      if (shouldBeValid) {
        t.expectValidationError(() => t.queue.submit([commandBuffer]), !submitShouldSucceedIfValid);
      }
    };

    this.validateFinishAndSubmitGivenState = resourceState => {
      this.validateFinishAndSubmit(resourceState !== 'invalid', resourceState !== 'destroyed');
    };
  }
}
