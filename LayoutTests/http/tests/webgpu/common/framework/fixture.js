/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert, unreachable } from '../util/util.js';

export class SkipTestCase extends Error {}
export class UnexpectedPassError extends Error {}

export { TestCaseRecorder } from '../internal/logging/test_case_recorder.js';

/** The fully-general type for params passed to a test function invocation. */

/**
 * A Fixture is a class used to instantiate each test sub/case at run time.
 * A new instance of the Fixture is created for every single test subcase
 * (i.e. every time the test function is run).
 */
export class Fixture {
  /**
   * Interface for recording logs and test status.
   *
   * @internal
   */

  eventualExpectations = [];
  numOutstandingAsyncExpectations = 0;
  objectsToCleanUp = [];

  /** @internal */
  constructor(rec, params) {
    this.rec = rec;
    this._params = params;
  }

  /**
   * Returns the (case+subcase) parameters for this test function invocation.
   */
  get params() {
    return this._params;
  }

  // This has to be a member function instead of an async `createFixture` function, because
  // we need to be able to ergonomically override it in subclasses.
  /**
   * Override this to do additional pre-test-function work in a derived fixture.
   */
  async init() {}

  /**
   * Override this to do additional post-test-function work in a derived fixture.
   *
   * Called even if init was unsuccessful.
   */
  async finalize() {
    assert(
      this.numOutstandingAsyncExpectations === 0,
      'there were outstanding immediateAsyncExpectations (e.g. expectUncapturedError) at the end of the test'
    );

    // Loop to exhaust the eventualExpectations in case they chain off each other.
    while (this.eventualExpectations.length) {
      const p = this.eventualExpectations.shift();
      try {
        await p;
      } catch (ex) {
        this.rec.threw(ex);
      }
    }

    // And clean up any objects now that they're done being used.
    for (const o of this.objectsToCleanUp) {
      if ('getExtension' in o) {
        const WEBGL_lose_context = o.getExtension('WEBGL_lose_context');
        if (WEBGL_lose_context) WEBGL_lose_context.loseContext();
      } else if ('destroy' in o) {
        o.destroy();
      } else {
        o.close();
      }
    }
  }

  /** @internal */
  doInit() {
    return this.init();
  }

  /** @internal */
  doFinalize() {
    return this.finalize();
  }

  /**
   * Tracks an object to be cleaned up after the test finishes.
   *
   * MAINTENANCE_TODO: Use this in more places. (Will be easier once .destroy() is allowed on
   * invalid objects.)
   */
  trackForCleanup(o) {
    this.objectsToCleanUp.push(o);
    return o;
  }

  /** Tracks an object, if it's destroyable, to be cleaned up after the test finishes. */
  tryTrackForCleanup(o) {
    if (typeof o === 'object' && o !== null) {
      if (
        'destroy' in o ||
        'close' in o ||
        o instanceof WebGLRenderingContext ||
        o instanceof WebGL2RenderingContext
      ) {
        this.objectsToCleanUp.push(o);
      }
    }
    return o;
  }

  /** Log a debug message. */
  debug(msg) {
    this.rec.debug(new Error(msg));
  }

  /** Throws an exception marking the subcase as skipped. */
  skip(msg) {
    throw new SkipTestCase(msg);
  }

  /** Log a warning and increase the result status to "Warn". */
  warn(msg) {
    this.rec.warn(new Error(msg));
  }

  /** Log an error and increase the result status to "ExpectFailed". */
  fail(msg) {
    this.rec.expectationFailed(new Error(msg));
  }

  /**
   * Wraps an async function. Tracks its status to fail if the test tries to report a test status
   * before the async work has finished.
   */
  async immediateAsyncExpectation(fn) {
    this.numOutstandingAsyncExpectations++;
    const ret = await fn();
    this.numOutstandingAsyncExpectations--;
    return ret;
  }

  /**
   * Wraps an async function, passing it an `Error` object recording the original stack trace.
   * The async work will be implicitly waited upon before reporting a test status.
   */
  eventualAsyncExpectation(fn) {
    const promise = fn(new Error());
    this.eventualExpectations.push(promise);
    return promise;
  }

  expectErrorValue(expectedError, ex, niceStack) {
    if (!(ex instanceof Error)) {
      niceStack.message = `THREW non-error value, of type ${typeof ex}: ${ex}`;
      this.rec.expectationFailed(niceStack);
      return;
    }
    const actualName = ex.name;
    if (expectedError !== true && actualName !== expectedError) {
      niceStack.message = `THREW ${actualName}, instead of ${expectedError}: ${ex}`;
      this.rec.expectationFailed(niceStack);
    } else {
      niceStack.message = `OK: threw ${actualName}: ${ex.message}`;
      this.rec.debug(niceStack);
    }
  }

  /** Expect that the provided promise resolves (fulfills). */
  shouldResolve(p, msg) {
    this.eventualAsyncExpectation(async niceStack => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        niceStack.message = 'resolved as expected' + m;
      } catch (ex) {
        niceStack.message = `REJECTED${m}`;
        if (ex instanceof Error) {
          niceStack.message += '\n' + ex.message;
        }
        this.rec.expectationFailed(niceStack);
      }
    });
  }

  /** Expect that the provided promise rejects, with the provided exception name. */
  shouldReject(expectedName, p, msg) {
    this.eventualAsyncExpectation(async niceStack => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        niceStack.message = 'DID NOT REJECT' + m;
        this.rec.expectationFailed(niceStack);
      } catch (ex) {
        niceStack.message = 'rejected as expected' + m;
        this.expectErrorValue(expectedName, ex, niceStack);
      }
    });
  }

  /**
   * Expect that the provided function throws.
   * If an `expectedName` is provided, expect that the throw exception has that name.
   */
  shouldThrow(expectedError, fn, msg) {
    const m = msg ? ': ' + msg : '';
    try {
      fn();
      if (expectedError === false) {
        this.rec.debug(new Error('did not throw, as expected' + m));
      } else {
        this.rec.expectationFailed(new Error('unexpectedly did not throw' + m));
      }
    } catch (ex) {
      if (expectedError === false) {
        this.rec.expectationFailed(new Error('threw unexpectedly' + m));
      } else {
        this.expectErrorValue(expectedError, ex, new Error(m));
      }
    }
  }

  /** Expect that a condition is true. */
  expect(cond, msg) {
    if (cond) {
      const m = msg ? ': ' + msg : '';
      this.rec.debug(new Error('expect OK' + m));
    } else {
      this.rec.expectationFailed(new Error(msg));
    }
    return cond;
  }

  /** If the argument is an `Error`, fail (or warn). If it's `undefined`, no-op. */
  expectOK(error, { mode = 'fail', niceStack } = {}) {
    if (error instanceof Error) {
      if (niceStack) {
        error.stack = niceStack.stack;
      }
      if (mode === 'fail') {
        this.rec.expectationFailed(error);
      } else if (mode === 'warn') {
        this.rec.warn(error);
      } else {
        unreachable();
      }
    }
  }

  eventualExpectOK(error, { mode = 'fail' } = {}) {
    this.eventualAsyncExpectation(async niceStack => {
      this.expectOK(await error, { mode, niceStack });
    });
  }
}
