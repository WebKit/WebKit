/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { ErrorWithExtra } from '../../../common/util/util.js';
import { GPUTest } from '../../gpu_test.js';
/**
 * Base fixture for WGSL shader validation tests.
 */
export class ShaderValidationTest extends GPUTest {
  /**
   * Add a test expectation for whether a createShaderModule call succeeds or not.
   *
   * @example
   * ```ts
   * t.expectCompileResult(true, `wgsl code`); // Expect success
   * t.expectCompileResult(false, `wgsl code`); // Expect validation error with any error string
   * t.expectCompileResult('substr', `wgsl code`); // Expect validation error containing 'substr'
   * ```
   *
   * MAINTENANCE_TODO(gpuweb/gpuweb#1813): Remove the "string" overload if there are no standard error codes.
   */
  expectCompileResult(expectedResult, code) {
    let shaderModule;
    this.expectGPUError(
      'validation',
      () => {
        shaderModule = this.device.createShaderModule({ code });
      },
      expectedResult !== true
    );

    const error = new ErrorWithExtra('', () => ({ shaderModule }));
    this.eventualAsyncExpectation(async () => {
      const compilationInfo = await shaderModule.compilationInfo();

      // MAINTENANCE_TODO: Pretty-print error messages with source context.
      const messagesLog = compilationInfo.messages
        .map(m => `${m.lineNum}:${m.linePos}: ${m.type}: ${m.message}`)
        .join('\n');
      error.extra.compilationInfo = compilationInfo;

      if (typeof expectedResult === 'string') {
        for (const msg of compilationInfo.messages) {
          if (msg.type === 'error' && msg.message.indexOf(expectedResult) !== -1) {
            error.message =
              `Found expected compilationInfo message substring «${expectedResult}».\n` +
              messagesLog;
            this.rec.debug(error);
            return;
          }
        }

        // Here, no error message was found, but one was expected.
        error.message = `Missing expected substring «${expectedResult}».\n` + messagesLog;
        this.rec.validationFailed(error);
        return;
      }

      if (compilationInfo.messages.some(m => m.type === 'error')) {
        if (expectedResult) {
          error.message = `Unexpected compilationInfo 'error' message.\n` + messagesLog;
          this.rec.validationFailed(error);
        } else {
          error.message = `Found expected compilationInfo 'error' message.\n` + messagesLog;
          this.rec.debug(error);
        }
      } else {
        if (!expectedResult) {
          error.message = `Missing expected compilationInfo 'error' message.\n` + messagesLog;
          this.rec.validationFailed(error);
        } else {
          error.message = `No compilationInfo 'error' messages, as expected.\n` + messagesLog;
          this.rec.debug(error);
        }
      }
    });
  }
}
