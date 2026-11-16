/******/ (() => { // webpackBootstrap
/******/ 	"use strict";
/******/ 	var __webpack_modules__ = ({

/***/ "./node_modules/assertion-error/index.js":
/*!***********************************************!*\
  !*** ./node_modules/assertion-error/index.js ***!
  \***********************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   AssertionError: () => (/* binding */ AssertionError),
/* harmony export */   AssertionResult: () => (/* binding */ AssertionResult)
/* harmony export */ });
// deno-fmt-ignore-file
// deno-lint-ignore-file
// This code was bundled using `deno bundle` and it's not recommended to edit it manually

const canElideFrames = "captureStackTrace" in Error;
class AssertionError extends Error {
    message;
    get name() {
        return "AssertionError";
    }
    get ok() {
        return false;
    }
    constructor(message = "Unspecified AssertionError", props, ssf){
        super(message);
        this.message = message;
        if (canElideFrames) {
            Error.captureStackTrace(this, ssf || AssertionError);
        }
        for(const key in props){
            if (!(key in this)) {
                this[key] = props[key];
            }
        }
    }
    toJSON(stack) {
        return {
            ...this,
            name: this.name,
            message: this.message,
            ok: false,
            stack: stack !== false ? this.stack : undefined
        };
    }
}
class AssertionResult {
    get name() {
        return "AssertionResult";
    }
    get ok() {
        return true;
    }
    constructor(props){
        for(const key in props){
            if (!(key in this)) {
                this[key] = props[key];
            }
        }
    }
    toJSON() {
        return {
            ...this,
            name: this.name,
            ok: this.ok
        };
    }
}





/***/ }),

/***/ "./node_modules/chai/index.js":
/*!************************************!*\
  !*** ./node_modules/chai/index.js ***!
  \************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   Assertion: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.Assertion),
/* harmony export */   AssertionError: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.AssertionError),
/* harmony export */   Should: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.Should),
/* harmony export */   assert: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.assert),
/* harmony export */   config: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.config),
/* harmony export */   expect: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.expect),
/* harmony export */   should: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.should),
/* harmony export */   use: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.use),
/* harmony export */   util: () => (/* reexport safe */ _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__.util)
/* harmony export */ });
/* harmony import */ var _lib_chai_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./lib/chai.js */ "./node_modules/chai/lib/chai.js");



/***/ }),

/***/ "./node_modules/chai/lib/chai.js":
/*!***************************************!*\
  !*** ./node_modules/chai/lib/chai.js ***!
  \***************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   Assertion: () => (/* reexport safe */ _chai_assertion_js__WEBPACK_IMPORTED_MODULE_5__.Assertion),
/* harmony export */   AssertionError: () => (/* reexport safe */ assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError),
/* harmony export */   Should: () => (/* reexport safe */ _chai_interface_should_js__WEBPACK_IMPORTED_MODULE_6__.Should),
/* harmony export */   assert: () => (/* reexport safe */ _chai_interface_assert_js__WEBPACK_IMPORTED_MODULE_7__.assert),
/* harmony export */   config: () => (/* reexport safe */ _chai_config_js__WEBPACK_IMPORTED_MODULE_2__.config),
/* harmony export */   expect: () => (/* reexport safe */ _chai_interface_expect_js__WEBPACK_IMPORTED_MODULE_4__.expect),
/* harmony export */   should: () => (/* reexport safe */ _chai_interface_should_js__WEBPACK_IMPORTED_MODULE_6__.should),
/* harmony export */   use: () => (/* binding */ use),
/* harmony export */   util: () => (/* reexport module object */ _chai_utils_index_js__WEBPACK_IMPORTED_MODULE_0__)
/* harmony export */ });
/* harmony import */ var _chai_utils_index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./chai/utils/index.js */ "./node_modules/chai/lib/chai/utils/index.js");
/* harmony import */ var assertion_error__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! assertion-error */ "./node_modules/assertion-error/index.js");
/* harmony import */ var _chai_config_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./chai/config.js */ "./node_modules/chai/lib/chai/config.js");
/* harmony import */ var _chai_core_assertions_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./chai/core/assertions.js */ "./node_modules/chai/lib/chai/core/assertions.js");
/* harmony import */ var _chai_interface_expect_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./chai/interface/expect.js */ "./node_modules/chai/lib/chai/interface/expect.js");
/* harmony import */ var _chai_assertion_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ./chai/assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var _chai_interface_should_js__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ./chai/interface/should.js */ "./node_modules/chai/lib/chai/interface/should.js");
/* harmony import */ var _chai_interface_assert_js__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./chai/interface/assert.js */ "./node_modules/chai/lib/chai/interface/assert.js");
/*!
 * chai
 * Copyright(c) 2011-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */










const used = [];

// Assertion Error


/**
 * # .use(function)
 *
 * Provides a way to extend the internals of Chai.
 *
 * @param {Function} fn
 * @returns {this} for chaining
 * @public
 */
function use(fn) {
  const exports = {
    use,
    AssertionError: assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError,
    util: _chai_utils_index_js__WEBPACK_IMPORTED_MODULE_0__,
    config: _chai_config_js__WEBPACK_IMPORTED_MODULE_2__.config,
    expect: _chai_interface_expect_js__WEBPACK_IMPORTED_MODULE_4__.expect,
    assert: _chai_interface_assert_js__WEBPACK_IMPORTED_MODULE_7__.assert,
    Assertion: _chai_assertion_js__WEBPACK_IMPORTED_MODULE_5__.Assertion,
    ..._chai_interface_should_js__WEBPACK_IMPORTED_MODULE_6__
  };

  if (!~used.indexOf(fn)) {
    fn(exports, _chai_utils_index_js__WEBPACK_IMPORTED_MODULE_0__);
    used.push(fn);
  }

  return exports;
}

// Utility Functions


// Configuration


// Primary `Assertion` prototype


// Expect interface


// Should interface


// Assert interface



/***/ }),

/***/ "./node_modules/chai/lib/chai/assertion.js":
/*!*************************************************!*\
  !*** ./node_modules/chai/lib/chai/assertion.js ***!
  \*************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   Assertion: () => (/* binding */ Assertion)
/* harmony export */ });
/* harmony import */ var _config_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./config.js */ "./node_modules/chai/lib/chai/config.js");
/* harmony import */ var assertion_error__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! assertion-error */ "./node_modules/assertion-error/index.js");
/* harmony import */ var _utils_index_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./utils/index.js */ "./node_modules/chai/lib/chai/utils/index.js");
/*!
 * chai
 * http://chaijs.com
 * Copyright(c) 2011-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */





class Assertion {
  /** @type {{}} */
  __flags = {};

  /**
   * Creates object for chaining.
   * `Assertion` objects contain metadata in the form of flags. Three flags can
   * be assigned during instantiation by passing arguments to this constructor:
   *
   * - `object`: This flag contains the target of the assertion. For example, in
   * the assertion `expect(numKittens).to.equal(7);`, the `object` flag will
   * contain `numKittens` so that the `equal` assertion can reference it when
   * needed.
   *
   * - `message`: This flag contains an optional custom error message to be
   * prepended to the error message that's generated by the assertion when it
   * fails.
   *
   * - `ssfi`: This flag stands for "start stack function indicator". It
   * contains a function reference that serves as the starting point for
   * removing frames from the stack trace of the error that's created by the
   * assertion when it fails. The goal is to provide a cleaner stack trace to
   * end users by removing Chai's internal functions. Note that it only works
   * in environments that support `Error.captureStackTrace`, and only when
   * `Chai.config.includeStack` hasn't been set to `false`.
   *
   * - `lockSsfi`: This flag controls whether or not the given `ssfi` flag
   * should retain its current value, even as assertions are chained off of
   * this object. This is usually set to `true` when creating a new assertion
   * from within another assertion. It's also temporarily set to `true` before
   * an overwritten assertion gets called by the overwriting assertion.
   *
   * - `eql`: This flag contains the deepEqual function to be used by the assertion.
   *
   * @param {unknown} obj target of the assertion
   * @param {string} [msg] (optional) custom error message
   * @param {Function} [ssfi] (optional) starting point for removing stack frames
   * @param {boolean} [lockSsfi] (optional) whether or not the ssfi flag is locked
   */
  constructor(obj, msg, ssfi, lockSsfi) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'ssfi', ssfi || Assertion);
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'lockSsfi', lockSsfi);
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'object', obj);
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'message', msg);
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'eql', _config_js__WEBPACK_IMPORTED_MODULE_0__.config.deepEqual || _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.eql);

    return _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.proxify(this);
  }

  /** @returns {boolean} */
  static get includeStack() {
    console.warn(
      'Assertion.includeStack is deprecated, use chai.config.includeStack instead.'
    );
    return _config_js__WEBPACK_IMPORTED_MODULE_0__.config.includeStack;
  }

  /** @param {boolean} value */
  static set includeStack(value) {
    console.warn(
      'Assertion.includeStack is deprecated, use chai.config.includeStack instead.'
    );
    _config_js__WEBPACK_IMPORTED_MODULE_0__.config.includeStack = value;
  }

  /** @returns {boolean} */
  static get showDiff() {
    console.warn(
      'Assertion.showDiff is deprecated, use chai.config.showDiff instead.'
    );
    return _config_js__WEBPACK_IMPORTED_MODULE_0__.config.showDiff;
  }

  /** @param {boolean} value */
  static set showDiff(value) {
    console.warn(
      'Assertion.showDiff is deprecated, use chai.config.showDiff instead.'
    );
    _config_js__WEBPACK_IMPORTED_MODULE_0__.config.showDiff = value;
  }

  /**
   * @param {string} name
   * @param {Function} fn
   */
  static addProperty(name, fn) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.addProperty(this.prototype, name, fn);
  }

  /**
   * @param {string} name
   * @param {Function} fn
   */
  static addMethod(name, fn) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.addMethod(this.prototype, name, fn);
  }

  /**
   * @param {string} name
   * @param {Function} fn
   * @param {Function} chainingBehavior
   */
  static addChainableMethod(name, fn, chainingBehavior) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.addChainableMethod(this.prototype, name, fn, chainingBehavior);
  }

  /**
   * @param {string} name
   * @param {Function} fn
   */
  static overwriteProperty(name, fn) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.overwriteProperty(this.prototype, name, fn);
  }

  /**
   * @param {string} name
   * @param {Function} fn
   */
  static overwriteMethod(name, fn) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.overwriteMethod(this.prototype, name, fn);
  }

  /**
   * @param {string} name
   * @param {Function} fn
   * @param {Function} chainingBehavior
   */
  static overwriteChainableMethod(name, fn, chainingBehavior) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.overwriteChainableMethod(this.prototype, name, fn, chainingBehavior);
  }

  /**
   * ### .assert(expression, message, negateMessage, expected, actual, showDiff)
   *
   * Executes an expression and check expectations. Throws AssertionError for reporting if test doesn't pass.
   *
   * @name assert
   * @param {unknown} _expr to be tested
   * @param {string | Function} msg or function that returns message to display if expression fails
   * @param {string | Function} _negateMsg or function that returns negatedMessage to display if negated expression fails
   * @param {unknown} expected value (remember to check for negation)
   * @param {unknown} _actual (optional) will default to `this.obj`
   * @param {boolean} showDiff (optional) when set to `true`, assert will display a diff in addition to the message if expression fails
   * @returns {void}
   */
  assert(_expr, msg, _negateMsg, expected, _actual, showDiff) {
    const ok = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.test(this, arguments);
    if (false !== showDiff) showDiff = true;
    if (undefined === expected && undefined === _actual) showDiff = false;
    if (true !== _config_js__WEBPACK_IMPORTED_MODULE_0__.config.showDiff) showDiff = false;

    if (!ok) {
      msg = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.getMessage(this, arguments);
      const actual = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.getActual(this, arguments);
      /** @type {Record<PropertyKey, unknown>} */
      const assertionErrorObjectProperties = {
        actual: actual,
        expected: expected,
        showDiff: showDiff
      };

      const operator = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.getOperator(this, arguments);
      if (operator) {
        assertionErrorObjectProperties.operator = operator;
      }

      throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
        msg,
        assertionErrorObjectProperties,
        // @ts-expect-error Not sure what to do about these types yet
        _config_js__WEBPACK_IMPORTED_MODULE_0__.config.includeStack ? this.assert : _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'ssfi')
      );
    }
  }

  /**
   * Quick reference to stored `actual` value for plugin developers.
   *
   * @returns {unknown}
   */
  get _obj() {
    return _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'object');
  }

  /**
   * Quick reference to stored `actual` value for plugin developers.
   *
   * @param {unknown} val
   */
  set _obj(val) {
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'object', val);
  }
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/config.js":
/*!**********************************************!*\
  !*** ./node_modules/chai/lib/chai/config.js ***!
  \**********************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   config: () => (/* binding */ config)
/* harmony export */ });
const config = {
  /**
   * ### config.includeStack
   *
   * User configurable property, influences whether stack trace
   * is included in Assertion error message. Default of false
   * suppresses stack trace in the error message.
   *
   *     chai.config.includeStack = true;  // enable stack on error
   *
   * @param {boolean}
   * @public
   */
  includeStack: false,

  /**
   * ### config.showDiff
   *
   * User configurable property, influences whether or not
   * the `showDiff` flag should be included in the thrown
   * AssertionErrors. `false` will always be `false`; `true`
   * will be true when the assertion has requested a diff
   * be shown.
   *
   * @param {boolean}
   * @public
   */
  showDiff: true,

  /**
   * ### config.truncateThreshold
   *
   * User configurable property, sets length threshold for actual and
   * expected values in assertion errors. If this threshold is exceeded, for
   * example for large data structures, the value is replaced with something
   * like `[ Array(3) ]` or `{ Object (prop1, prop2) }`.
   *
   * Set it to zero if you want to disable truncating altogether.
   *
   * This is especially userful when doing assertions on arrays: having this
   * set to a reasonable large value makes the failure messages readily
   * inspectable.
   *
   *     chai.config.truncateThreshold = 0;  // disable truncating
   *
   * @param {number}
   * @public
   */
  truncateThreshold: 40,

  /**
   * ### config.useProxy
   *
   * User configurable property, defines if chai will use a Proxy to throw
   * an error when a non-existent property is read, which protects users
   * from typos when using property-based assertions.
   *
   * Set it to false if you want to disable this feature.
   *
   *     chai.config.useProxy = false;  // disable use of Proxy
   *
   * This feature is automatically disabled regardless of this config value
   * in environments that don't support proxies.
   *
   * @param {boolean}
   * @public
   */
  useProxy: true,

  /**
   * ### config.proxyExcludedKeys
   *
   * User configurable property, defines which properties should be ignored
   * instead of throwing an error if they do not exist on the assertion.
   * This is only applied if the environment Chai is running in supports proxies and
   * if the `useProxy` configuration setting is enabled.
   * By default, `then` and `inspect` will not throw an error if they do not exist on the
   * assertion object because the `.inspect` property is read by `util.inspect` (for example, when
   * using `console.log` on the assertion object) and `.then` is necessary for promise type-checking.
   *
   *     // By default these keys will not throw an error if they do not exist on the assertion object
   *     chai.config.proxyExcludedKeys = ['then', 'inspect'];
   *
   * @param {Array}
   * @public
   */
  proxyExcludedKeys: ['then', 'catch', 'inspect', 'toJSON'],

  /**
   * ### config.deepEqual
   *
   * User configurable property, defines which a custom function to use for deepEqual
   * comparisons.
   * By default, the function used is the one from the `deep-eql` package without custom comparator.
   *
   *     // use a custom comparator
   *     chai.config.deepEqual = (expected, actual) => {
   *         return chai.util.eql(expected, actual, {
   *             comparator: (expected, actual) => {
   *                 // for non number comparison, use the default behavior
   *                 if(typeof expected !== 'number') return null;
   *                 // allow a difference of 10 between compared numbers
   *                 return typeof actual === 'number' && Math.abs(actual - expected) < 10
   *             }
   *         })
   *     };
   *
   * @param {Function}
   * @public
   */
  deepEqual: null
};


/***/ }),

/***/ "./node_modules/chai/lib/chai/core/assertions.js":
/*!*******************************************************!*\
  !*** ./node_modules/chai/lib/chai/core/assertions.js ***!
  \*******************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var assertion_error__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! assertion-error */ "./node_modules/assertion-error/index.js");
/* harmony import */ var _utils_index_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../utils/index.js */ "./node_modules/chai/lib/chai/utils/index.js");
/* harmony import */ var _config_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../config.js */ "./node_modules/chai/lib/chai/config.js");
/*!
 * chai
 * http://chaijs.com
 * Copyright(c) 2011-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */






const {flag} = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__;

/**
 * ### Language Chains
 *
 * The following are provided as chainable getters to improve the readability
 * of your assertions.
 *
 * **Chains**
 *
 * - to
 * - be
 * - been
 * - is
 * - that
 * - which
 * - and
 * - has
 * - have
 * - with
 * - at
 * - of
 * - same
 * - but
 * - does
 * - still
 * - also
 *
 * @name language chains
 * @namespace BDD
 * @public
 */

[
  'to',
  'be',
  'been',
  'is',
  'and',
  'has',
  'have',
  'with',
  'that',
  'which',
  'at',
  'of',
  'same',
  'but',
  'does',
  'still',
  'also'
].forEach(function (chain) {
  _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty(chain);
});

/**
 * ### .not
 *
 * Negates all assertions that follow in the chain.
 *
 *     expect(function () {}).to.not.throw();
 *     expect({a: 1}).to.not.have.property('b');
 *     expect([1, 2]).to.be.an('array').that.does.not.include(3);
 *
 * Just because you can negate any assertion with `.not` doesn't mean you
 * should. With great power comes great responsibility. It's often best to
 * assert that the one expected output was produced, rather than asserting
 * that one of countless unexpected outputs wasn't produced. See individual
 * assertions for specific guidance.
 *
 *     expect(2).to.equal(2); // Recommended
 *     expect(2).to.not.equal(1); // Not recommended
 *
 * @name not
 * @namespace BDD
 * @public
 */

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('not', function () {
  flag(this, 'negate', true);
});

/**
 * ### .deep
 *
 * Causes all `.equal`, `.include`, `.members`, `.keys`, and `.property`
 * assertions that follow in the chain to use deep equality instead of strict
 * (`===`) equality. See the `deep-eql` project page for info on the deep
 * equality algorithm: https://github.com/chaijs/deep-eql.
 *
 *     // Target object deeply (but not strictly) equals `{a: 1}`
 *     expect({a: 1}).to.deep.equal({a: 1});
 *     expect({a: 1}).to.not.equal({a: 1});
 *
 *     // Target array deeply (but not strictly) includes `{a: 1}`
 *     expect([{a: 1}]).to.deep.include({a: 1});
 *     expect([{a: 1}]).to.not.include({a: 1});
 *
 *     // Target object deeply (but not strictly) includes `x: {a: 1}`
 *     expect({x: {a: 1}}).to.deep.include({x: {a: 1}});
 *     expect({x: {a: 1}}).to.not.include({x: {a: 1}});
 *
 *     // Target array deeply (but not strictly) has member `{a: 1}`
 *     expect([{a: 1}]).to.have.deep.members([{a: 1}]);
 *     expect([{a: 1}]).to.not.have.members([{a: 1}]);
 *
 *     // Target set deeply (but not strictly) has key `{a: 1}`
 *     expect(new Set([{a: 1}])).to.have.deep.keys([{a: 1}]);
 *     expect(new Set([{a: 1}])).to.not.have.keys([{a: 1}]);
 *
 *     // Target object deeply (but not strictly) has property `x: {a: 1}`
 *     expect({x: {a: 1}}).to.have.deep.property('x', {a: 1});
 *     expect({x: {a: 1}}).to.not.have.property('x', {a: 1});
 *
 * @name deep
 * @namespace BDD
 * @public
 */

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('deep', function () {
  flag(this, 'deep', true);
});

/**
 * ### .nested
 *
 * Enables dot- and bracket-notation in all `.property` and `.include`
 * assertions that follow in the chain.
 *
 *     expect({a: {b: ['x', 'y']}}).to.have.nested.property('a.b[1]');
 *     expect({a: {b: ['x', 'y']}}).to.nested.include({'a.b[1]': 'y'});
 *
 * If `.` or `[]` are part of an actual property name, they can be escaped by
 * adding two backslashes before them.
 *
 *     expect({'.a': {'[b]': 'x'}}).to.have.nested.property('\\.a.\\[b\\]');
 *     expect({'.a': {'[b]': 'x'}}).to.nested.include({'\\.a.\\[b\\]': 'x'});
 *
 * `.nested` cannot be combined with `.own`.
 *
 * @name nested
 * @namespace BDD
 * @public
 */

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('nested', function () {
  flag(this, 'nested', true);
});

/**
 * ### .own
 *
 * Causes all `.property` and `.include` assertions that follow in the chain
 * to ignore inherited properties.
 *
 *     Object.prototype.b = 2;
 *
 *     expect({a: 1}).to.have.own.property('a');
 *     expect({a: 1}).to.have.property('b');
 *     expect({a: 1}).to.not.have.own.property('b');
 *
 *     expect({a: 1}).to.own.include({a: 1});
 *     expect({a: 1}).to.include({b: 2}).but.not.own.include({b: 2});
 *
 * `.own` cannot be combined with `.nested`.
 *
 * @name own
 * @namespace BDD
 * @public
 */

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('own', function () {
  flag(this, 'own', true);
});

/**
 * ### .ordered
 *
 * Causes all `.members` assertions that follow in the chain to require that
 * members be in the same order.
 *
 *     expect([1, 2]).to.have.ordered.members([1, 2])
 *       .but.not.have.ordered.members([2, 1]);
 *
 * When `.include` and `.ordered` are combined, the ordering begins at the
 * start of both arrays.
 *
 *     expect([1, 2, 3]).to.include.ordered.members([1, 2])
 *       .but.not.include.ordered.members([2, 3]);
 *
 * @name ordered
 * @namespace BDD
 * @public
 */

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('ordered', function () {
  flag(this, 'ordered', true);
});

/**
 * ### .any
 *
 * Causes all `.keys` assertions that follow in the chain to only require that
 * the target have at least one of the given keys. This is the opposite of
 * `.all`, which requires that the target have all of the given keys.
 *
 *     expect({a: 1, b: 2}).to.not.have.any.keys('c', 'd');
 *
 * See the `.keys` doc for guidance on when to use `.any` or `.all`.
 *
 * @name any
 * @namespace BDD
 * @public
 */

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('any', function () {
  flag(this, 'any', true);
  flag(this, 'all', false);
});

/**
 * ### .all
 *
 * Causes all `.keys` assertions that follow in the chain to require that the
 * target have all of the given keys. This is the opposite of `.any`, which
 * only requires that the target have at least one of the given keys.
 *
 *     expect({a: 1, b: 2}).to.have.all.keys('a', 'b');
 *
 * Note that `.all` is used by default when neither `.all` nor `.any` are
 * added earlier in the chain. However, it's often best to add `.all` anyway
 * because it improves readability.
 *
 * See the `.keys` doc for guidance on when to use `.any` or `.all`.
 *
 * @name all
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('all', function () {
  flag(this, 'all', true);
  flag(this, 'any', false);
});

const functionTypes = {
  function: [
    'function',
    'asyncfunction',
    'generatorfunction',
    'asyncgeneratorfunction'
  ],
  asyncfunction: ['asyncfunction', 'asyncgeneratorfunction'],
  generatorfunction: ['generatorfunction', 'asyncgeneratorfunction'],
  asyncgeneratorfunction: ['asyncgeneratorfunction']
};

/**
 * ### .a(type[, msg])
 *
 * Asserts that the target's type is equal to the given string `type`. Types
 * are case insensitive. See the utility file `./type-detect.js` for info on the
 * type detection algorithm.
 *
 *     expect('foo').to.be.a('string');
 *     expect({a: 1}).to.be.an('object');
 *     expect(null).to.be.a('null');
 *     expect(undefined).to.be.an('undefined');
 *     expect(new Error).to.be.an('error');
 *     expect(Promise.resolve()).to.be.a('promise');
 *     expect(new Float32Array).to.be.a('float32array');
 *     expect(Symbol()).to.be.a('symbol');
 *
 * `.a` supports objects that have a custom type set via `Symbol.toStringTag`.
 *
 *     var myObj = {
 *         [Symbol.toStringTag]: 'myCustomType'
 *     };
 *
 *     expect(myObj).to.be.a('myCustomType').but.not.an('object');
 *
 * It's often best to use `.a` to check a target's type before making more
 * assertions on the same target. That way, you avoid unexpected behavior from
 * any assertion that does different things based on the target's type.
 *
 *     expect([1, 2, 3]).to.be.an('array').that.includes(2);
 *     expect([]).to.be.an('array').that.is.empty;
 *
 * Add `.not` earlier in the chain to negate `.a`. However, it's often best to
 * assert that the target is the expected type, rather than asserting that it
 * isn't one of many unexpected types.
 *
 *     expect('foo').to.be.a('string'); // Recommended
 *     expect('foo').to.not.be.an('array'); // Not recommended
 *
 * `.a` accepts an optional `msg` argument which is a custom error message to
 * show when the assertion fails. The message can also be given as the second
 * argument to `expect`.
 *
 *     expect(1).to.be.a('string', 'nooo why fail??');
 *     expect(1, 'nooo why fail??').to.be.a('string');
 *
 * `.a` can also be used as a language chain to improve the readability of
 * your assertions.
 *
 *     expect({b: 2}).to.have.a.property('b');
 *
 * The alias `.an` can be used interchangeably with `.a`.
 *
 * @name a
 * @alias an
 * @param {string} type
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function an(type, msg) {
  if (msg) flag(this, 'message', msg);
  type = type.toLowerCase();
  let obj = flag(this, 'object'),
    article = ~['a', 'e', 'i', 'o', 'u'].indexOf(type.charAt(0)) ? 'an ' : 'a ';

  const detectedType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase();

  if (functionTypes['function'].includes(type)) {
    this.assert(
      functionTypes[type].includes(detectedType),
      'expected #{this} to be ' + article + type,
      'expected #{this} not to be ' + article + type
    );
  } else {
    this.assert(
      type === detectedType,
      'expected #{this} to be ' + article + type,
      'expected #{this} not to be ' + article + type
    );
  }
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('an', an);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('a', an);

/**
 * @param {unknown} a
 * @param {unknown} b
 * @returns {boolean}
 */
function SameValueZero(a, b) {
  return (_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNaN(a) && _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNaN(b)) || a === b;
}

/** */
function includeChainingBehavior() {
  flag(this, 'contains', true);
}

/**
 * ### .include(val[, msg])
 *
 * When the target is a string, `.include` asserts that the given string `val`
 * is a substring of the target.
 *
 *     expect('foobar').to.include('foo');
 *
 * When the target is an array, `.include` asserts that the given `val` is a
 * member of the target.
 *
 *     expect([1, 2, 3]).to.include(2);
 *
 * When the target is an object, `.include` asserts that the given object
 * `val`'s properties are a subset of the target's properties.
 *
 *     expect({a: 1, b: 2, c: 3}).to.include({a: 1, b: 2});
 *
 * When the target is a Set or WeakSet, `.include` asserts that the given `val` is a
 * member of the target. SameValueZero equality algorithm is used.
 *
 *     expect(new Set([1, 2])).to.include(2);
 *
 * When the target is a Map, `.include` asserts that the given `val` is one of
 * the values of the target. SameValueZero equality algorithm is used.
 *
 *     expect(new Map([['a', 1], ['b', 2]])).to.include(2);
 *
 * Because `.include` does different things based on the target's type, it's
 * important to check the target's type before using `.include`. See the `.a`
 * doc for info on testing a target's type.
 *
 *     expect([1, 2, 3]).to.be.an('array').that.includes(2);
 *
 * By default, strict (`===`) equality is used to compare array members and
 * object properties. Add `.deep` earlier in the chain to use deep equality
 * instead (WeakSet targets are not supported). See the `deep-eql` project
 * page for info on the deep equality algorithm: https://github.com/chaijs/deep-eql.
 *
 *     // Target array deeply (but not strictly) includes `{a: 1}`
 *     expect([{a: 1}]).to.deep.include({a: 1});
 *     expect([{a: 1}]).to.not.include({a: 1});
 *
 *     // Target object deeply (but not strictly) includes `x: {a: 1}`
 *     expect({x: {a: 1}}).to.deep.include({x: {a: 1}});
 *     expect({x: {a: 1}}).to.not.include({x: {a: 1}});
 *
 * By default, all of the target's properties are searched when working with
 * objects. This includes properties that are inherited and/or non-enumerable.
 * Add `.own` earlier in the chain to exclude the target's inherited
 * properties from the search.
 *
 *     Object.prototype.b = 2;
 *
 *     expect({a: 1}).to.own.include({a: 1});
 *     expect({a: 1}).to.include({b: 2}).but.not.own.include({b: 2});
 *
 * Note that a target object is always only searched for `val`'s own
 * enumerable properties.
 *
 * `.deep` and `.own` can be combined.
 *
 *     expect({a: {b: 2}}).to.deep.own.include({a: {b: 2}});
 *
 * Add `.nested` earlier in the chain to enable dot- and bracket-notation when
 * referencing nested properties.
 *
 *     expect({a: {b: ['x', 'y']}}).to.nested.include({'a.b[1]': 'y'});
 *
 * If `.` or `[]` are part of an actual property name, they can be escaped by
 * adding two backslashes before them.
 *
 *     expect({'.a': {'[b]': 2}}).to.nested.include({'\\.a.\\[b\\]': 2});
 *
 * `.deep` and `.nested` can be combined.
 *
 *     expect({a: {b: [{c: 3}]}}).to.deep.nested.include({'a.b[0]': {c: 3}});
 *
 * `.own` and `.nested` cannot be combined.
 *
 * Add `.not` earlier in the chain to negate `.include`.
 *
 *     expect('foobar').to.not.include('taco');
 *     expect([1, 2, 3]).to.not.include(4);
 *
 * However, it's dangerous to negate `.include` when the target is an object.
 * The problem is that it creates uncertain expectations by asserting that the
 * target object doesn't have all of `val`'s key/value pairs but may or may
 * not have some of them. It's often best to identify the exact output that's
 * expected, and then write an assertion that only accepts that exact output.
 *
 * When the target object isn't even expected to have `val`'s keys, it's
 * often best to assert exactly that.
 *
 *     expect({c: 3}).to.not.have.any.keys('a', 'b'); // Recommended
 *     expect({c: 3}).to.not.include({a: 1, b: 2}); // Not recommended
 *
 * When the target object is expected to have `val`'s keys, it's often best to
 * assert that each of the properties has its expected value, rather than
 * asserting that each property doesn't have one of many unexpected values.
 *
 *     expect({a: 3, b: 4}).to.include({a: 3, b: 4}); // Recommended
 *     expect({a: 3, b: 4}).to.not.include({a: 1, b: 2}); // Not recommended
 *
 * `.include` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect([1, 2, 3]).to.include(4, 'nooo why fail??');
 *     expect([1, 2, 3], 'nooo why fail??').to.include(4);
 *
 * `.include` can also be used as a language chain, causing all `.members` and
 * `.keys` assertions that follow in the chain to require the target to be a
 * superset of the expected set, rather than an identical set. Note that
 * `.members` ignores duplicates in the subset when `.include` is added.
 *
 *     // Target object's keys are a superset of ['a', 'b'] but not identical
 *     expect({a: 1, b: 2, c: 3}).to.include.all.keys('a', 'b');
 *     expect({a: 1, b: 2, c: 3}).to.not.have.all.keys('a', 'b');
 *
 *     // Target array is a superset of [1, 2] but not identical
 *     expect([1, 2, 3]).to.include.members([1, 2]);
 *     expect([1, 2, 3]).to.not.have.members([1, 2]);
 *
 *     // Duplicates in the subset are ignored
 *     expect([1, 2, 3]).to.include.members([1, 2, 2, 2]);
 *
 * Note that adding `.any` earlier in the chain causes the `.keys` assertion
 * to ignore `.include`.
 *
 *     // Both assertions are identical
 *     expect({a: 1}).to.include.any.keys('a', 'b');
 *     expect({a: 1}).to.have.any.keys('a', 'b');
 *
 * The aliases `.includes`, `.contain`, and `.contains` can be used
 * interchangeably with `.include`.
 *
 * @name include
 * @alias contain
 * @alias includes
 * @alias contains
 * @param {unknown} val
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function include(val, msg) {
  if (msg) flag(this, 'message', msg);

  let obj = flag(this, 'object'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase(),
    flagMsg = flag(this, 'message'),
    negate = flag(this, 'negate'),
    ssfi = flag(this, 'ssfi'),
    isDeep = flag(this, 'deep'),
    descriptor = isDeep ? 'deep ' : '',
    isEql = isDeep ? flag(this, 'eql') : SameValueZero;

  flagMsg = flagMsg ? flagMsg + ': ' : '';

  let included = false;

  switch (objType) {
    case 'string':
      included = obj.indexOf(val) !== -1;
      break;

    case 'weakset':
      if (isDeep) {
        throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
          flagMsg + 'unable to use .deep.include with WeakSet',
          undefined,
          ssfi
        );
      }

      included = obj.has(val);
      break;

    case 'map':
      obj.forEach(function (item) {
        included = included || isEql(item, val);
      });
      break;

    case 'set':
      if (isDeep) {
        obj.forEach(function (item) {
          included = included || isEql(item, val);
        });
      } else {
        included = obj.has(val);
      }
      break;

    case 'array':
      if (isDeep) {
        included = obj.some(function (item) {
          return isEql(item, val);
        });
      } else {
        included = obj.indexOf(val) !== -1;
      }
      break;

    default: {
      // This block is for asserting a subset of properties in an object.
      // `_.expectTypes` isn't used here because `.include` should work with
      // objects with a custom `@@toStringTag`.
      if (val !== Object(val)) {
        throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
          flagMsg +
            'the given combination of arguments (' +
            objType +
            ' and ' +
            _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(val).toLowerCase() +
            ')' +
            ' is invalid for this assertion. ' +
            'You can use an array, a map, an object, a set, a string, ' +
            'or a weakset instead of a ' +
            _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(val).toLowerCase(),
          undefined,
          ssfi
        );
      }

      let props = Object.keys(val);
      let firstErr = null;
      let numErrs = 0;

      props.forEach(function (prop) {
        let propAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj);
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.transferFlags(this, propAssertion, true);
        flag(propAssertion, 'lockSsfi', true);

        if (!negate || props.length === 1) {
          propAssertion.property(prop, val[prop]);
          return;
        }

        try {
          propAssertion.property(prop, val[prop]);
        } catch (err) {
          if (!_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.compatibleConstructor(err, assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError)) {
            throw err;
          }
          if (firstErr === null) firstErr = err;
          numErrs++;
        }
      }, this);

      // When validating .not.include with multiple properties, we only want
      // to throw an assertion error if all of the properties are included,
      // in which case we throw the first property assertion error that we
      // encountered.
      if (negate && props.length > 1 && numErrs === props.length) {
        throw firstErr;
      }
      return;
    }
  }

  // Assert inclusion in collection or substring in a string.
  this.assert(
    included,
    'expected #{this} to ' + descriptor + 'include ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(val),
    'expected #{this} to not ' + descriptor + 'include ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(val)
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('include', include, includeChainingBehavior);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('contain', include, includeChainingBehavior);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('contains', include, includeChainingBehavior);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('includes', include, includeChainingBehavior);

/**
 * ### .ok
 *
 * Asserts that the target is a truthy value (considered `true` in boolean context).
 * However, it's often best to assert that the target is strictly (`===`) or
 * deeply equal to its expected value.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.be.ok; // Not recommended
 *
 *     expect(true).to.be.true; // Recommended
 *     expect(true).to.be.ok; // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.ok`.
 *
 *     expect(0).to.equal(0); // Recommended
 *     expect(0).to.not.be.ok; // Not recommended
 *
 *     expect(false).to.be.false; // Recommended
 *     expect(false).to.not.be.ok; // Not recommended
 *
 *     expect(null).to.be.null; // Recommended
 *     expect(null).to.not.be.ok; // Not recommended
 *
 *     expect(undefined).to.be.undefined; // Recommended
 *     expect(undefined).to.not.be.ok; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(false, 'nooo why fail??').to.be.ok;
 *
 * @name ok
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('ok', function () {
  this.assert(
    flag(this, 'object'),
    'expected #{this} to be truthy',
    'expected #{this} to be falsy'
  );
});

/**
 * ### .true
 *
 * Asserts that the target is strictly (`===`) equal to `true`.
 *
 *     expect(true).to.be.true;
 *
 * Add `.not` earlier in the chain to negate `.true`. However, it's often best
 * to assert that the target is equal to its expected value, rather than not
 * equal to `true`.
 *
 *     expect(false).to.be.false; // Recommended
 *     expect(false).to.not.be.true; // Not recommended
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.be.true; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(false, 'nooo why fail??').to.be.true;
 *
 * @name true
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('true', function () {
  this.assert(
    true === flag(this, 'object'),
    'expected #{this} to be true',
    'expected #{this} to be false',
    flag(this, 'negate') ? false : true
  );
});

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('numeric', function () {
  const object = flag(this, 'object');

  this.assert(
    ['Number', 'BigInt'].includes(_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(object)),
    'expected #{this} to be numeric',
    'expected #{this} to not be numeric',
    flag(this, 'negate') ? false : true
  );
});

/**
 * ### .callable
 *
 * Asserts that the target a callable function.
 *
 *     expect(console.log).to.be.callable;
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect('not a function', 'nooo why fail??').to.be.callable;
 *
 * @name callable
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('callable', function () {
  const val = flag(this, 'object');
  const ssfi = flag(this, 'ssfi');
  const message = flag(this, 'message');
  const msg = message ? `${message}: ` : '';
  const negate = flag(this, 'negate');

  const assertionMessage = negate
    ? `${msg}expected ${_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(val)} not to be a callable function`
    : `${msg}expected ${_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(val)} to be a callable function`;

  const isCallable = [
    'Function',
    'AsyncFunction',
    'GeneratorFunction',
    'AsyncGeneratorFunction'
  ].includes(_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(val));

  if ((isCallable && negate) || (!isCallable && !negate)) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(assertionMessage, undefined, ssfi);
  }
});

/**
 * ### .false
 *
 * Asserts that the target is strictly (`===`) equal to `false`.
 *
 *     expect(false).to.be.false;
 *
 * Add `.not` earlier in the chain to negate `.false`. However, it's often
 * best to assert that the target is equal to its expected value, rather than
 * not equal to `false`.
 *
 *     expect(true).to.be.true; // Recommended
 *     expect(true).to.not.be.false; // Not recommended
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.be.false; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(true, 'nooo why fail??').to.be.false;
 *
 * @name false
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('false', function () {
  this.assert(
    false === flag(this, 'object'),
    'expected #{this} to be false',
    'expected #{this} to be true',
    flag(this, 'negate') ? true : false
  );
});

/**
 * ### .null
 *
 * Asserts that the target is strictly (`===`) equal to `null`.
 *
 *     expect(null).to.be.null;
 *
 * Add `.not` earlier in the chain to negate `.null`. However, it's often best
 * to assert that the target is equal to its expected value, rather than not
 * equal to `null`.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.be.null; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(42, 'nooo why fail??').to.be.null;
 *
 * @name null
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('null', function () {
  this.assert(
    null === flag(this, 'object'),
    'expected #{this} to be null',
    'expected #{this} not to be null'
  );
});

/**
 * ### .undefined
 *
 * Asserts that the target is strictly (`===`) equal to `undefined`.
 *
 *     expect(undefined).to.be.undefined;
 *
 * Add `.not` earlier in the chain to negate `.undefined`. However, it's often
 * best to assert that the target is equal to its expected value, rather than
 * not equal to `undefined`.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.be.undefined; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(42, 'nooo why fail??').to.be.undefined;
 *
 * @name undefined
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('undefined', function () {
  this.assert(
    undefined === flag(this, 'object'),
    'expected #{this} to be undefined',
    'expected #{this} not to be undefined'
  );
});

/**
 * ### .NaN
 *
 * Asserts that the target is exactly `NaN`.
 *
 *     expect(NaN).to.be.NaN;
 *
 * Add `.not` earlier in the chain to negate `.NaN`. However, it's often best
 * to assert that the target is equal to its expected value, rather than not
 * equal to `NaN`.
 *
 *     expect('foo').to.equal('foo'); // Recommended
 *     expect('foo').to.not.be.NaN; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(42, 'nooo why fail??').to.be.NaN;
 *
 * @name NaN
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('NaN', function () {
  this.assert(
    _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNaN(flag(this, 'object')),
    'expected #{this} to be NaN',
    'expected #{this} not to be NaN'
  );
});

/**
 * ### .exist
 *
 * Asserts that the target is not strictly (`===`) equal to either `null` or
 * `undefined`. However, it's often best to assert that the target is equal to
 * its expected value.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.exist; // Not recommended
 *
 *     expect(0).to.equal(0); // Recommended
 *     expect(0).to.exist; // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.exist`.
 *
 *     expect(null).to.be.null; // Recommended
 *     expect(null).to.not.exist; // Not recommended
 *
 *     expect(undefined).to.be.undefined; // Recommended
 *     expect(undefined).to.not.exist; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(null, 'nooo why fail??').to.exist;
 *
 * The alias `.exists` can be used interchangeably with `.exist`.
 *
 * @name exist
 * @alias exists
 * @namespace BDD
 * @public
 */
function assertExist() {
  let val = flag(this, 'object');
  this.assert(
    val !== null && val !== undefined,
    'expected #{this} to exist',
    'expected #{this} to not exist'
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('exist', assertExist);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('exists', assertExist);

/**
 * ### .empty
 *
 * When the target is a string or array, `.empty` asserts that the target's
 * `length` property is strictly (`===`) equal to `0`.
 *
 *     expect([]).to.be.empty;
 *     expect('').to.be.empty;
 *
 * When the target is a map or set, `.empty` asserts that the target's `size`
 * property is strictly equal to `0`.
 *
 *     expect(new Set()).to.be.empty;
 *     expect(new Map()).to.be.empty;
 *
 * When the target is a non-function object, `.empty` asserts that the target
 * doesn't have any own enumerable properties. Properties with Symbol-based
 * keys are excluded from the count.
 *
 *     expect({}).to.be.empty;
 *
 * Because `.empty` does different things based on the target's type, it's
 * important to check the target's type before using `.empty`. See the `.a`
 * doc for info on testing a target's type.
 *
 *     expect([]).to.be.an('array').that.is.empty;
 *
 * Add `.not` earlier in the chain to negate `.empty`. However, it's often
 * best to assert that the target contains its expected number of values,
 * rather than asserting that it's not empty.
 *
 *     expect([1, 2, 3]).to.have.lengthOf(3); // Recommended
 *     expect([1, 2, 3]).to.not.be.empty; // Not recommended
 *
 *     expect(new Set([1, 2, 3])).to.have.property('size', 3); // Recommended
 *     expect(new Set([1, 2, 3])).to.not.be.empty; // Not recommended
 *
 *     expect(Object.keys({a: 1})).to.have.lengthOf(1); // Recommended
 *     expect({a: 1}).to.not.be.empty; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect([1, 2, 3], 'nooo why fail??').to.be.empty;
 *
 * @name empty
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('empty', function () {
  let val = flag(this, 'object'),
    ssfi = flag(this, 'ssfi'),
    flagMsg = flag(this, 'message'),
    itemsCount;

  flagMsg = flagMsg ? flagMsg + ': ' : '';

  switch (_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(val).toLowerCase()) {
    case 'array':
    case 'string':
      itemsCount = val.length;
      break;
    case 'map':
    case 'set':
      itemsCount = val.size;
      break;
    case 'weakmap':
    case 'weakset':
      throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
        flagMsg + '.empty was passed a weak collection',
        undefined,
        ssfi
      );
    case 'function': {
      const msg = flagMsg + '.empty was passed a function ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.getName(val);
      throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(msg.trim(), undefined, ssfi);
    }
    default:
      if (val !== Object(val)) {
        throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
          flagMsg + '.empty was passed non-string primitive ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(val),
          undefined,
          ssfi
        );
      }
      itemsCount = Object.keys(val).length;
  }

  this.assert(
    0 === itemsCount,
    'expected #{this} to be empty',
    'expected #{this} not to be empty'
  );
});

/**
 * ### .arguments
 *
 * Asserts that the target is an `arguments` object.
 *
 *     function test () {
 *         expect(arguments).to.be.arguments;
 *     }
 *
 *     test();
 *
 * Add `.not` earlier in the chain to negate `.arguments`. However, it's often
 * best to assert which type the target is expected to be, rather than
 * asserting that it’s not an `arguments` object.
 *
 *     expect('foo').to.be.a('string'); // Recommended
 *     expect('foo').to.not.be.arguments; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect({}, 'nooo why fail??').to.be.arguments;
 *
 * The alias `.Arguments` can be used interchangeably with `.arguments`.
 *
 * @name arguments
 * @alias Arguments
 * @namespace BDD
 * @public
 */
function checkArguments() {
  let obj = flag(this, 'object'),
    type = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj);
  this.assert(
    'Arguments' === type,
    'expected #{this} to be arguments but got ' + type,
    'expected #{this} to not be arguments'
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('arguments', checkArguments);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('Arguments', checkArguments);

/**
 * ### .equal(val[, msg])
 *
 * Asserts that the target is strictly (`===`) equal to the given `val`.
 *
 *     expect(1).to.equal(1);
 *     expect('foo').to.equal('foo');
 *
 * Add `.deep` earlier in the chain to use deep equality instead. See the
 * `deep-eql` project page for info on the deep equality algorithm:
 * https://github.com/chaijs/deep-eql.
 *
 *     // Target object deeply (but not strictly) equals `{a: 1}`
 *     expect({a: 1}).to.deep.equal({a: 1});
 *     expect({a: 1}).to.not.equal({a: 1});
 *
 *     // Target array deeply (but not strictly) equals `[1, 2]`
 *     expect([1, 2]).to.deep.equal([1, 2]);
 *     expect([1, 2]).to.not.equal([1, 2]);
 *
 * Add `.not` earlier in the chain to negate `.equal`. However, it's often
 * best to assert that the target is equal to its expected value, rather than
 * not equal to one of countless unexpected values.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.equal(2); // Not recommended
 *
 * `.equal` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect(1).to.equal(2, 'nooo why fail??');
 *     expect(1, 'nooo why fail??').to.equal(2);
 *
 * The aliases `.equals` and `eq` can be used interchangeably with `.equal`.
 *
 * @name equal
 * @alias equals
 * @alias eq
 * @param {unknown} val
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertEqual(val, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object');
  if (flag(this, 'deep')) {
    let prevLockSsfi = flag(this, 'lockSsfi');
    flag(this, 'lockSsfi', true);
    this.eql(val);
    flag(this, 'lockSsfi', prevLockSsfi);
  } else {
    this.assert(
      val === obj,
      'expected #{this} to equal #{exp}',
      'expected #{this} to not equal #{exp}',
      val,
      this._obj,
      true
    );
  }
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('equal', assertEqual);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('equals', assertEqual);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('eq', assertEqual);

/**
 * ### .eql(obj[, msg])
 *
 * Asserts that the target is deeply equal to the given `obj`. See the
 * `deep-eql` project page for info on the deep equality algorithm:
 * https://github.com/chaijs/deep-eql.
 *
 *     // Target object is deeply (but not strictly) equal to {a: 1}
 *     expect({a: 1}).to.eql({a: 1}).but.not.equal({a: 1});
 *
 *     // Target array is deeply (but not strictly) equal to [1, 2]
 *     expect([1, 2]).to.eql([1, 2]).but.not.equal([1, 2]);
 *
 * Add `.not` earlier in the chain to negate `.eql`. However, it's often best
 * to assert that the target is deeply equal to its expected value, rather
 * than not deeply equal to one of countless unexpected values.
 *
 *     expect({a: 1}).to.eql({a: 1}); // Recommended
 *     expect({a: 1}).to.not.eql({b: 2}); // Not recommended
 *
 * `.eql` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect({a: 1}).to.eql({b: 2}, 'nooo why fail??');
 *     expect({a: 1}, 'nooo why fail??').to.eql({b: 2});
 *
 * The alias `.eqls` can be used interchangeably with `.eql`.
 *
 * The `.deep.equal` assertion is almost identical to `.eql` but with one
 * difference: `.deep.equal` causes deep equality comparisons to also be used
 * for any other assertions that follow in the chain.
 *
 * @name eql
 * @alias eqls
 * @param {unknown} obj
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertEql(obj, msg) {
  if (msg) flag(this, 'message', msg);
  let eql = flag(this, 'eql');
  this.assert(
    eql(obj, flag(this, 'object')),
    'expected #{this} to deeply equal #{exp}',
    'expected #{this} to not deeply equal #{exp}',
    obj,
    this._obj,
    true
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('eql', assertEql);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('eqls', assertEql);

/**
 * ### .above(n[, msg])
 *
 * Asserts that the target is a number or a date greater than the given number or date `n` respectively.
 * However, it's often best to assert that the target is equal to its expected
 * value.
 *
 *     expect(2).to.equal(2); // Recommended
 *     expect(2).to.be.above(1); // Not recommended
 *
 * Add `.lengthOf` earlier in the chain to assert that the target's `length`
 * or `size` is greater than the given number `n`.
 *
 *     expect('foo').to.have.lengthOf(3); // Recommended
 *     expect('foo').to.have.lengthOf.above(2); // Not recommended
 *
 *     expect([1, 2, 3]).to.have.lengthOf(3); // Recommended
 *     expect([1, 2, 3]).to.have.lengthOf.above(2); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.above`.
 *
 *     expect(2).to.equal(2); // Recommended
 *     expect(1).to.not.be.above(2); // Not recommended
 *
 * `.above` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect(1).to.be.above(2, 'nooo why fail??');
 *     expect(1, 'nooo why fail??').to.be.above(2);
 *
 * The aliases `.gt` and `.greaterThan` can be used interchangeably with
 * `.above`.
 *
 * @name above
 * @alias gt
 * @alias greaterThan
 * @param {number} n
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertAbove(n, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    doLength = flag(this, 'doLength'),
    flagMsg = flag(this, 'message'),
    msgPrefix = flagMsg ? flagMsg + ': ' : '',
    ssfi = flag(this, 'ssfi'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase(),
    nType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(n).toLowerCase();

  if (doLength && objType !== 'map' && objType !== 'set') {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).to.have.property('length');
  }

  if (!doLength && objType === 'date' && nType !== 'date') {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      msgPrefix + 'the argument to above must be a date',
      undefined,
      ssfi
    );
  } else if (!_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(n) && (doLength || _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj))) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      msgPrefix + 'the argument to above must be a number',
      undefined,
      ssfi
    );
  } else if (!doLength && objType !== 'date' && !_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj)) {
    let printObj = objType === 'string' ? "'" + obj + "'" : obj;
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      msgPrefix + 'expected ' + printObj + ' to be a number or a date',
      undefined,
      ssfi
    );
  }

  if (doLength) {
    let descriptor = 'length',
      itemsCount;
    if (objType === 'map' || objType === 'set') {
      descriptor = 'size';
      itemsCount = obj.size;
    } else {
      itemsCount = obj.length;
    }
    this.assert(
      itemsCount > n,
      'expected #{this} to have a ' +
        descriptor +
        ' above #{exp} but got #{act}',
      'expected #{this} to not have a ' + descriptor + ' above #{exp}',
      n,
      itemsCount
    );
  } else {
    this.assert(
      obj > n,
      'expected #{this} to be above #{exp}',
      'expected #{this} to be at most #{exp}',
      n
    );
  }
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('above', assertAbove);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('gt', assertAbove);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('greaterThan', assertAbove);

/**
 * ### .least(n[, msg])
 *
 * Asserts that the target is a number or a date greater than or equal to the given
 * number or date `n` respectively. However, it's often best to assert that the target is equal to
 * its expected value.
 *
 *     expect(2).to.equal(2); // Recommended
 *     expect(2).to.be.at.least(1); // Not recommended
 *     expect(2).to.be.at.least(2); // Not recommended
 *
 * Add `.lengthOf` earlier in the chain to assert that the target's `length`
 * or `size` is greater than or equal to the given number `n`.
 *
 *     expect('foo').to.have.lengthOf(3); // Recommended
 *     expect('foo').to.have.lengthOf.at.least(2); // Not recommended
 *
 *     expect([1, 2, 3]).to.have.lengthOf(3); // Recommended
 *     expect([1, 2, 3]).to.have.lengthOf.at.least(2); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.least`.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.be.at.least(2); // Not recommended
 *
 * `.least` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect(1).to.be.at.least(2, 'nooo why fail??');
 *     expect(1, 'nooo why fail??').to.be.at.least(2);
 *
 * The aliases `.gte` and `.greaterThanOrEqual` can be used interchangeably with
 * `.least`.
 *
 * @name least
 * @alias gte
 * @alias greaterThanOrEqual
 * @param {unknown} n
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertLeast(n, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    doLength = flag(this, 'doLength'),
    flagMsg = flag(this, 'message'),
    msgPrefix = flagMsg ? flagMsg + ': ' : '',
    ssfi = flag(this, 'ssfi'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase(),
    nType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(n).toLowerCase(),
    errorMessage,
    shouldThrow = true;

  if (doLength && objType !== 'map' && objType !== 'set') {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).to.have.property('length');
  }

  if (!doLength && objType === 'date' && nType !== 'date') {
    errorMessage = msgPrefix + 'the argument to least must be a date';
  } else if (!_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(n) && (doLength || _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj))) {
    errorMessage = msgPrefix + 'the argument to least must be a number';
  } else if (!doLength && objType !== 'date' && !_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj)) {
    let printObj = objType === 'string' ? "'" + obj + "'" : obj;
    errorMessage =
      msgPrefix + 'expected ' + printObj + ' to be a number or a date';
  } else {
    shouldThrow = false;
  }

  if (shouldThrow) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(errorMessage, undefined, ssfi);
  }

  if (doLength) {
    let descriptor = 'length',
      itemsCount;
    if (objType === 'map' || objType === 'set') {
      descriptor = 'size';
      itemsCount = obj.size;
    } else {
      itemsCount = obj.length;
    }
    this.assert(
      itemsCount >= n,
      'expected #{this} to have a ' +
        descriptor +
        ' at least #{exp} but got #{act}',
      'expected #{this} to have a ' + descriptor + ' below #{exp}',
      n,
      itemsCount
    );
  } else {
    this.assert(
      obj >= n,
      'expected #{this} to be at least #{exp}',
      'expected #{this} to be below #{exp}',
      n
    );
  }
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('least', assertLeast);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('gte', assertLeast);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('greaterThanOrEqual', assertLeast);

/**
 * ### .below(n[, msg])
 *
 * Asserts that the target is a number or a date less than the given number or date `n` respectively.
 * However, it's often best to assert that the target is equal to its expected
 * value.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.be.below(2); // Not recommended
 *
 * Add `.lengthOf` earlier in the chain to assert that the target's `length`
 * or `size` is less than the given number `n`.
 *
 *     expect('foo').to.have.lengthOf(3); // Recommended
 *     expect('foo').to.have.lengthOf.below(4); // Not recommended
 *
 *     expect([1, 2, 3]).to.have.length(3); // Recommended
 *     expect([1, 2, 3]).to.have.lengthOf.below(4); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.below`.
 *
 *     expect(2).to.equal(2); // Recommended
 *     expect(2).to.not.be.below(1); // Not recommended
 *
 * `.below` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect(2).to.be.below(1, 'nooo why fail??');
 *     expect(2, 'nooo why fail??').to.be.below(1);
 *
 * The aliases `.lt` and `.lessThan` can be used interchangeably with
 * `.below`.
 *
 * @name below
 * @alias lt
 * @alias lessThan
 * @param {unknown} n
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertBelow(n, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    doLength = flag(this, 'doLength'),
    flagMsg = flag(this, 'message'),
    msgPrefix = flagMsg ? flagMsg + ': ' : '',
    ssfi = flag(this, 'ssfi'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase(),
    nType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(n).toLowerCase(),
    errorMessage,
    shouldThrow = true;

  if (doLength && objType !== 'map' && objType !== 'set') {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).to.have.property('length');
  }

  if (!doLength && objType === 'date' && nType !== 'date') {
    errorMessage = msgPrefix + 'the argument to below must be a date';
  } else if (!_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(n) && (doLength || _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj))) {
    errorMessage = msgPrefix + 'the argument to below must be a number';
  } else if (!doLength && objType !== 'date' && !_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj)) {
    let printObj = objType === 'string' ? "'" + obj + "'" : obj;
    errorMessage =
      msgPrefix + 'expected ' + printObj + ' to be a number or a date';
  } else {
    shouldThrow = false;
  }

  if (shouldThrow) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(errorMessage, undefined, ssfi);
  }

  if (doLength) {
    let descriptor = 'length',
      itemsCount;
    if (objType === 'map' || objType === 'set') {
      descriptor = 'size';
      itemsCount = obj.size;
    } else {
      itemsCount = obj.length;
    }
    this.assert(
      itemsCount < n,
      'expected #{this} to have a ' +
        descriptor +
        ' below #{exp} but got #{act}',
      'expected #{this} to not have a ' + descriptor + ' below #{exp}',
      n,
      itemsCount
    );
  } else {
    this.assert(
      obj < n,
      'expected #{this} to be below #{exp}',
      'expected #{this} to be at least #{exp}',
      n
    );
  }
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('below', assertBelow);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('lt', assertBelow);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('lessThan', assertBelow);

/**
 * ### .most(n[, msg])
 *
 * Asserts that the target is a number or a date less than or equal to the given number
 * or date `n` respectively. However, it's often best to assert that the target is equal to its
 * expected value.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.be.at.most(2); // Not recommended
 *     expect(1).to.be.at.most(1); // Not recommended
 *
 * Add `.lengthOf` earlier in the chain to assert that the target's `length`
 * or `size` is less than or equal to the given number `n`.
 *
 *     expect('foo').to.have.lengthOf(3); // Recommended
 *     expect('foo').to.have.lengthOf.at.most(4); // Not recommended
 *
 *     expect([1, 2, 3]).to.have.lengthOf(3); // Recommended
 *     expect([1, 2, 3]).to.have.lengthOf.at.most(4); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.most`.
 *
 *     expect(2).to.equal(2); // Recommended
 *     expect(2).to.not.be.at.most(1); // Not recommended
 *
 * `.most` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect(2).to.be.at.most(1, 'nooo why fail??');
 *     expect(2, 'nooo why fail??').to.be.at.most(1);
 *
 * The aliases `.lte` and `.lessThanOrEqual` can be used interchangeably with
 * `.most`.
 *
 * @name most
 * @alias lte
 * @alias lessThanOrEqual
 * @param {unknown} n
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertMost(n, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    doLength = flag(this, 'doLength'),
    flagMsg = flag(this, 'message'),
    msgPrefix = flagMsg ? flagMsg + ': ' : '',
    ssfi = flag(this, 'ssfi'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase(),
    nType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(n).toLowerCase(),
    errorMessage,
    shouldThrow = true;

  if (doLength && objType !== 'map' && objType !== 'set') {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).to.have.property('length');
  }

  if (!doLength && objType === 'date' && nType !== 'date') {
    errorMessage = msgPrefix + 'the argument to most must be a date';
  } else if (!_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(n) && (doLength || _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj))) {
    errorMessage = msgPrefix + 'the argument to most must be a number';
  } else if (!doLength && objType !== 'date' && !_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj)) {
    let printObj = objType === 'string' ? "'" + obj + "'" : obj;
    errorMessage =
      msgPrefix + 'expected ' + printObj + ' to be a number or a date';
  } else {
    shouldThrow = false;
  }

  if (shouldThrow) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(errorMessage, undefined, ssfi);
  }

  if (doLength) {
    let descriptor = 'length',
      itemsCount;
    if (objType === 'map' || objType === 'set') {
      descriptor = 'size';
      itemsCount = obj.size;
    } else {
      itemsCount = obj.length;
    }
    this.assert(
      itemsCount <= n,
      'expected #{this} to have a ' +
        descriptor +
        ' at most #{exp} but got #{act}',
      'expected #{this} to have a ' + descriptor + ' above #{exp}',
      n,
      itemsCount
    );
  } else {
    this.assert(
      obj <= n,
      'expected #{this} to be at most #{exp}',
      'expected #{this} to be above #{exp}',
      n
    );
  }
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('most', assertMost);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('lte', assertMost);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('lessThanOrEqual', assertMost);

/**
 * ### .within(start, finish[, msg])
 *
 * Asserts that the target is a number or a date greater than or equal to the given
 * number or date `start`, and less than or equal to the given number or date `finish` respectively.
 * However, it's often best to assert that the target is equal to its expected
 * value.
 *
 *     expect(2).to.equal(2); // Recommended
 *     expect(2).to.be.within(1, 3); // Not recommended
 *     expect(2).to.be.within(2, 3); // Not recommended
 *     expect(2).to.be.within(1, 2); // Not recommended
 *
 * Add `.lengthOf` earlier in the chain to assert that the target's `length`
 * or `size` is greater than or equal to the given number `start`, and less
 * than or equal to the given number `finish`.
 *
 *     expect('foo').to.have.lengthOf(3); // Recommended
 *     expect('foo').to.have.lengthOf.within(2, 4); // Not recommended
 *
 *     expect([1, 2, 3]).to.have.lengthOf(3); // Recommended
 *     expect([1, 2, 3]).to.have.lengthOf.within(2, 4); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.within`.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.be.within(2, 4); // Not recommended
 *
 * `.within` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect(4).to.be.within(1, 3, 'nooo why fail??');
 *     expect(4, 'nooo why fail??').to.be.within(1, 3);
 *
 * @name within
 * @param {unknown} start lower bound inclusive
 * @param {unknown} finish upper bound inclusive
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('within', function (start, finish, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    doLength = flag(this, 'doLength'),
    flagMsg = flag(this, 'message'),
    msgPrefix = flagMsg ? flagMsg + ': ' : '',
    ssfi = flag(this, 'ssfi'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase(),
    startType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(start).toLowerCase(),
    finishType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(finish).toLowerCase(),
    errorMessage,
    shouldThrow = true,
    range =
      startType === 'date' && finishType === 'date'
        ? start.toISOString() + '..' + finish.toISOString()
        : start + '..' + finish;

  if (doLength && objType !== 'map' && objType !== 'set') {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).to.have.property('length');
  }

  if (
    !doLength &&
    objType === 'date' &&
    (startType !== 'date' || finishType !== 'date')
  ) {
    errorMessage = msgPrefix + 'the arguments to within must be dates';
  } else if (
    (!_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(start) || !_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(finish)) &&
    (doLength || _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj))
  ) {
    errorMessage = msgPrefix + 'the arguments to within must be numbers';
  } else if (!doLength && objType !== 'date' && !_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isNumeric(obj)) {
    let printObj = objType === 'string' ? "'" + obj + "'" : obj;
    errorMessage =
      msgPrefix + 'expected ' + printObj + ' to be a number or a date';
  } else {
    shouldThrow = false;
  }

  if (shouldThrow) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(errorMessage, undefined, ssfi);
  }

  if (doLength) {
    let descriptor = 'length',
      itemsCount;
    if (objType === 'map' || objType === 'set') {
      descriptor = 'size';
      itemsCount = obj.size;
    } else {
      itemsCount = obj.length;
    }
    this.assert(
      itemsCount >= start && itemsCount <= finish,
      'expected #{this} to have a ' + descriptor + ' within ' + range,
      'expected #{this} to not have a ' + descriptor + ' within ' + range
    );
  } else {
    this.assert(
      obj >= start && obj <= finish,
      'expected #{this} to be within ' + range,
      'expected #{this} to not be within ' + range
    );
  }
});

/**
 * ### .instanceof(constructor[, msg])
 *
 * Asserts that the target is an instance of the given `constructor`.
 *
 *     function Cat () { }
 *
 *     expect(new Cat()).to.be.an.instanceof(Cat);
 *     expect([1, 2]).to.be.an.instanceof(Array);
 *
 * Add `.not` earlier in the chain to negate `.instanceof`.
 *
 *     expect({a: 1}).to.not.be.an.instanceof(Array);
 *
 * `.instanceof` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect(1).to.be.an.instanceof(Array, 'nooo why fail??');
 *     expect(1, 'nooo why fail??').to.be.an.instanceof(Array);
 *
 * Due to limitations in ES5, `.instanceof` may not always work as expected
 * when using a transpiler such as Babel or TypeScript. In particular, it may
 * produce unexpected results when subclassing built-in object such as
 * `Array`, `Error`, and `Map`. See your transpiler's docs for details:
 *
 * - ([Babel](https://babeljs.io/docs/usage/caveats/#classes))
 * - ([TypeScript](https://github.com/Microsoft/TypeScript/wiki/Breaking-Changes#extending-built-ins-like-error-array-and-map-may-no-longer-work))
 *
 * The alias `.instanceOf` can be used interchangeably with `.instanceof`.
 *
 * @name instanceof
 * @param {unknown} constructor
 * @param {string} msg _optional_
 * @alias instanceOf
 * @namespace BDD
 * @public
 */
function assertInstanceOf(constructor, msg) {
  if (msg) flag(this, 'message', msg);

  let target = flag(this, 'object');
  let ssfi = flag(this, 'ssfi');
  let flagMsg = flag(this, 'message');
  let isInstanceOf;

  try {
    isInstanceOf = target instanceof constructor;
  } catch (err) {
    if (err instanceof TypeError) {
      flagMsg = flagMsg ? flagMsg + ': ' : '';
      throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
        flagMsg +
          'The instanceof assertion needs a constructor but ' +
          _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(constructor) +
          ' was given.',
        undefined,
        ssfi
      );
    }
    throw err;
  }

  let name = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.getName(constructor);
  if (name == null) {
    name = 'an unnamed constructor';
  }

  this.assert(
    isInstanceOf,
    'expected #{this} to be an instance of ' + name,
    'expected #{this} to not be an instance of ' + name
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('instanceof', assertInstanceOf);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('instanceOf', assertInstanceOf);

/**
 * ### .property(name[, val[, msg]])
 *
 * Asserts that the target has a property with the given key `name`.
 *
 *     expect({a: 1}).to.have.property('a');
 *
 * When `val` is provided, `.property` also asserts that the property's value
 * is equal to the given `val`.
 *
 *     expect({a: 1}).to.have.property('a', 1);
 *
 * By default, strict (`===`) equality is used. Add `.deep` earlier in the
 * chain to use deep equality instead. See the `deep-eql` project page for
 * info on the deep equality algorithm: https://github.com/chaijs/deep-eql.
 *
 *     // Target object deeply (but not strictly) has property `x: {a: 1}`
 *     expect({x: {a: 1}}).to.have.deep.property('x', {a: 1});
 *     expect({x: {a: 1}}).to.not.have.property('x', {a: 1});
 *
 * The target's enumerable and non-enumerable properties are always included
 * in the search. By default, both own and inherited properties are included.
 * Add `.own` earlier in the chain to exclude inherited properties from the
 * search.
 *
 *     Object.prototype.b = 2;
 *
 *     expect({a: 1}).to.have.own.property('a');
 *     expect({a: 1}).to.have.own.property('a', 1);
 *     expect({a: 1}).to.have.property('b');
 *     expect({a: 1}).to.not.have.own.property('b');
 *
 * `.deep` and `.own` can be combined.
 *
 *     expect({x: {a: 1}}).to.have.deep.own.property('x', {a: 1});
 *
 * Add `.nested` earlier in the chain to enable dot- and bracket-notation when
 * referencing nested properties.
 *
 *     expect({a: {b: ['x', 'y']}}).to.have.nested.property('a.b[1]');
 *     expect({a: {b: ['x', 'y']}}).to.have.nested.property('a.b[1]', 'y');
 *
 * If `.` or `[]` are part of an actual property name, they can be escaped by
 * adding two backslashes before them.
 *
 *     expect({'.a': {'[b]': 'x'}}).to.have.nested.property('\\.a.\\[b\\]');
 *
 * `.deep` and `.nested` can be combined.
 *
 *     expect({a: {b: [{c: 3}]}})
 *       .to.have.deep.nested.property('a.b[0]', {c: 3});
 *
 * `.own` and `.nested` cannot be combined.
 *
 * Add `.not` earlier in the chain to negate `.property`.
 *
 *     expect({a: 1}).to.not.have.property('b');
 *
 * However, it's dangerous to negate `.property` when providing `val`. The
 * problem is that it creates uncertain expectations by asserting that the
 * target either doesn't have a property with the given key `name`, or that it
 * does have a property with the given key `name` but its value isn't equal to
 * the given `val`. It's often best to identify the exact output that's
 * expected, and then write an assertion that only accepts that exact output.
 *
 * When the target isn't expected to have a property with the given key
 * `name`, it's often best to assert exactly that.
 *
 *     expect({b: 2}).to.not.have.property('a'); // Recommended
 *     expect({b: 2}).to.not.have.property('a', 1); // Not recommended
 *
 * When the target is expected to have a property with the given key `name`,
 * it's often best to assert that the property has its expected value, rather
 * than asserting that it doesn't have one of many unexpected values.
 *
 *     expect({a: 3}).to.have.property('a', 3); // Recommended
 *     expect({a: 3}).to.not.have.property('a', 1); // Not recommended
 *
 * `.property` changes the target of any assertions that follow in the chain
 * to be the value of the property from the original target object.
 *
 *     expect({a: 1}).to.have.property('a').that.is.a('number');
 *
 * `.property` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`. When not providing `val`, only use the
 * second form.
 *
 *     // Recommended
 *     expect({a: 1}).to.have.property('a', 2, 'nooo why fail??');
 *     expect({a: 1}, 'nooo why fail??').to.have.property('a', 2);
 *     expect({a: 1}, 'nooo why fail??').to.have.property('b');
 *
 *     // Not recommended
 *     expect({a: 1}).to.have.property('b', undefined, 'nooo why fail??');
 *
 * The above assertion isn't the same thing as not providing `val`. Instead,
 * it's asserting that the target object has a `b` property that's equal to
 * `undefined`.
 *
 * The assertions `.ownProperty` and `.haveOwnProperty` can be used
 * interchangeably with `.own.property`.
 *
 * @name property
 * @param {string} name
 * @param {unknown} val (optional)
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertProperty(name, val, msg) {
  if (msg) flag(this, 'message', msg);

  let isNested = flag(this, 'nested'),
    isOwn = flag(this, 'own'),
    flagMsg = flag(this, 'message'),
    obj = flag(this, 'object'),
    ssfi = flag(this, 'ssfi'),
    nameType = typeof name;

  flagMsg = flagMsg ? flagMsg + ': ' : '';

  if (isNested) {
    if (nameType !== 'string') {
      throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
        flagMsg +
          'the argument to property must be a string when using nested syntax',
        undefined,
        ssfi
      );
    }
  } else {
    if (
      nameType !== 'string' &&
      nameType !== 'number' &&
      nameType !== 'symbol'
    ) {
      throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
        flagMsg +
          'the argument to property must be a string, number, or symbol',
        undefined,
        ssfi
      );
    }
  }

  if (isNested && isOwn) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      flagMsg + 'The "nested" and "own" flags cannot be combined.',
      undefined,
      ssfi
    );
  }

  if (obj === null || obj === undefined) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      flagMsg + 'Target cannot be null or undefined.',
      undefined,
      ssfi
    );
  }

  let isDeep = flag(this, 'deep'),
    negate = flag(this, 'negate'),
    pathInfo = isNested ? _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.getPathInfo(obj, name) : null,
    value = isNested ? pathInfo.value : obj[name],
    isEql = isDeep ? flag(this, 'eql') : (val1, val2) => val1 === val2;

  let descriptor = '';
  if (isDeep) descriptor += 'deep ';
  if (isOwn) descriptor += 'own ';
  if (isNested) descriptor += 'nested ';
  descriptor += 'property ';

  let hasProperty;
  if (isOwn) hasProperty = Object.prototype.hasOwnProperty.call(obj, name);
  else if (isNested) hasProperty = pathInfo.exists;
  else hasProperty = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.hasProperty(obj, name);

  // When performing a negated assertion for both name and val, merely having
  // a property with the given name isn't enough to cause the assertion to
  // fail. It must both have a property with the given name, and the value of
  // that property must equal the given val. Therefore, skip this assertion in
  // favor of the next.
  if (!negate || arguments.length === 1) {
    this.assert(
      hasProperty,
      'expected #{this} to have ' + descriptor + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name),
      'expected #{this} to not have ' + descriptor + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name)
    );
  }

  if (arguments.length > 1) {
    this.assert(
      hasProperty && isEql(val, value),
      'expected #{this} to have ' +
        descriptor +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name) +
        ' of #{exp}, but got #{act}',
      'expected #{this} to not have ' +
        descriptor +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name) +
        ' of #{act}',
      val,
      value
    );
  }

  flag(this, 'object', value);
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('property', assertProperty);

/**
 * @param {unknown} _name
 * @param {unknown} _value
 * @param {string} _msg
 */
function assertOwnProperty(_name, _value, _msg) {
  flag(this, 'own', true);
  assertProperty.apply(this, arguments);
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('ownProperty', assertOwnProperty);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('haveOwnProperty', assertOwnProperty);

/**
 * ### .ownPropertyDescriptor(name[, descriptor[, msg]])
 *
 * Asserts that the target has its own property descriptor with the given key
 * `name`. Enumerable and non-enumerable properties are included in the
 * search.
 *
 *     expect({a: 1}).to.have.ownPropertyDescriptor('a');
 *
 * When `descriptor` is provided, `.ownPropertyDescriptor` also asserts that
 * the property's descriptor is deeply equal to the given `descriptor`. See
 * the `deep-eql` project page for info on the deep equality algorithm:
 * https://github.com/chaijs/deep-eql.
 *
 *     expect({a: 1}).to.have.ownPropertyDescriptor('a', {
 *         configurable: true,
 *         enumerable: true,
 *         writable: true,
 *         value: 1,
 *     });
 *
 * Add `.not` earlier in the chain to negate `.ownPropertyDescriptor`.
 *
 *     expect({a: 1}).to.not.have.ownPropertyDescriptor('b');
 *
 * However, it's dangerous to negate `.ownPropertyDescriptor` when providing
 * a `descriptor`. The problem is that it creates uncertain expectations by
 * asserting that the target either doesn't have a property descriptor with
 * the given key `name`, or that it does have a property descriptor with the
 * given key `name` but it’s not deeply equal to the given `descriptor`. It's
 * often best to identify the exact output that's expected, and then write an
 * assertion that only accepts that exact output.
 *
 * When the target isn't expected to have a property descriptor with the given
 * key `name`, it's often best to assert exactly that.
 *
 *     // Recommended
 *     expect({b: 2}).to.not.have.ownPropertyDescriptor('a');
 *
 *     // Not recommended
 *     expect({b: 2}).to.not.have.ownPropertyDescriptor('a', {
 *         configurable: true,
 *         enumerable: true,
 *         writable: true,
 *         value: 1,
 *     });
 *
 * When the target is expected to have a property descriptor with the given
 * key `name`, it's often best to assert that the property has its expected
 * descriptor, rather than asserting that it doesn't have one of many
 * unexpected descriptors.
 *
 *     // Recommended
 *     expect({a: 3}).to.have.ownPropertyDescriptor('a', {
 *         configurable: true,
 *         enumerable: true,
 *         writable: true,
 *         value: 3,
 *     });
 *
 *     // Not recommended
 *     expect({a: 3}).to.not.have.ownPropertyDescriptor('a', {
 *         configurable: true,
 *         enumerable: true,
 *         writable: true,
 *         value: 1,
 *     });
 *
 * `.ownPropertyDescriptor` changes the target of any assertions that follow
 * in the chain to be the value of the property descriptor from the original
 * target object.
 *
 *     expect({a: 1}).to.have.ownPropertyDescriptor('a')
 *       .that.has.property('enumerable', true);
 *
 * `.ownPropertyDescriptor` accepts an optional `msg` argument which is a
 * custom error message to show when the assertion fails. The message can also
 * be given as the second argument to `expect`. When not providing
 * `descriptor`, only use the second form.
 *
 *     // Recommended
 *     expect({a: 1}).to.have.ownPropertyDescriptor('a', {
 *         configurable: true,
 *         enumerable: true,
 *         writable: true,
 *         value: 2,
 *     }, 'nooo why fail??');
 *
 *     // Recommended
 *     expect({a: 1}, 'nooo why fail??').to.have.ownPropertyDescriptor('a', {
 *         configurable: true,
 *         enumerable: true,
 *         writable: true,
 *         value: 2,
 *     });
 *
 *     // Recommended
 *     expect({a: 1}, 'nooo why fail??').to.have.ownPropertyDescriptor('b');
 *
 *     // Not recommended
 *     expect({a: 1})
 *       .to.have.ownPropertyDescriptor('b', undefined, 'nooo why fail??');
 *
 * The above assertion isn't the same thing as not providing `descriptor`.
 * Instead, it's asserting that the target object has a `b` property
 * descriptor that's deeply equal to `undefined`.
 *
 * The alias `.haveOwnPropertyDescriptor` can be used interchangeably with
 * `.ownPropertyDescriptor`.
 *
 * @name ownPropertyDescriptor
 * @alias haveOwnPropertyDescriptor
 * @param {string} name
 * @param {object} descriptor _optional_
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertOwnPropertyDescriptor(name, descriptor, msg) {
  if (typeof descriptor === 'string') {
    msg = descriptor;
    descriptor = null;
  }
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object');
  let actualDescriptor = Object.getOwnPropertyDescriptor(Object(obj), name);
  let eql = flag(this, 'eql');
  if (actualDescriptor && descriptor) {
    this.assert(
      eql(descriptor, actualDescriptor),
      'expected the own property descriptor for ' +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name) +
        ' on #{this} to match ' +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(descriptor) +
        ', got ' +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(actualDescriptor),
      'expected the own property descriptor for ' +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name) +
        ' on #{this} to not match ' +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(descriptor),
      descriptor,
      actualDescriptor,
      true
    );
  } else {
    this.assert(
      actualDescriptor,
      'expected #{this} to have an own property descriptor for ' +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name),
      'expected #{this} to not have an own property descriptor for ' +
        _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(name)
    );
  }
  flag(this, 'object', actualDescriptor);
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('ownPropertyDescriptor', assertOwnPropertyDescriptor);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('haveOwnPropertyDescriptor', assertOwnPropertyDescriptor);

/** */
function assertLengthChain() {
  flag(this, 'doLength', true);
}

/**
 * ### .lengthOf(n[, msg])
 *
 * Asserts that the target's `length` or `size` is equal to the given number
 * `n`.
 *
 *     expect([1, 2, 3]).to.have.lengthOf(3);
 *     expect('foo').to.have.lengthOf(3);
 *     expect(new Set([1, 2, 3])).to.have.lengthOf(3);
 *     expect(new Map([['a', 1], ['b', 2], ['c', 3]])).to.have.lengthOf(3);
 *
 * Add `.not` earlier in the chain to negate `.lengthOf`. However, it's often
 * best to assert that the target's `length` property is equal to its expected
 * value, rather than not equal to one of many unexpected values.
 *
 *     expect('foo').to.have.lengthOf(3); // Recommended
 *     expect('foo').to.not.have.lengthOf(4); // Not recommended
 *
 * `.lengthOf` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect([1, 2, 3]).to.have.lengthOf(2, 'nooo why fail??');
 *     expect([1, 2, 3], 'nooo why fail??').to.have.lengthOf(2);
 *
 * `.lengthOf` can also be used as a language chain, causing all `.above`,
 * `.below`, `.least`, `.most`, and `.within` assertions that follow in the
 * chain to use the target's `length` property as the target. However, it's
 * often best to assert that the target's `length` property is equal to its
 * expected length, rather than asserting that its `length` property falls
 * within some range of values.
 *
 *     // Recommended
 *     expect([1, 2, 3]).to.have.lengthOf(3);
 *
 *     // Not recommended
 *     expect([1, 2, 3]).to.have.lengthOf.above(2);
 *     expect([1, 2, 3]).to.have.lengthOf.below(4);
 *     expect([1, 2, 3]).to.have.lengthOf.at.least(3);
 *     expect([1, 2, 3]).to.have.lengthOf.at.most(3);
 *     expect([1, 2, 3]).to.have.lengthOf.within(2,4);
 *
 * Due to a compatibility issue, the alias `.length` can't be chained directly
 * off of an uninvoked method such as `.a`. Therefore, `.length` can't be used
 * interchangeably with `.lengthOf` in every situation. It's recommended to
 * always use `.lengthOf` instead of `.length`.
 *
 *     expect([1, 2, 3]).to.have.a.length(3); // incompatible; throws error
 *     expect([1, 2, 3]).to.have.a.lengthOf(3);  // passes as expected
 *
 * @name lengthOf
 * @alias length
 * @param {number} n
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertLength(n, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj).toLowerCase(),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi'),
    descriptor = 'length',
    itemsCount;

  switch (objType) {
    case 'map':
    case 'set':
      descriptor = 'size';
      itemsCount = obj.size;
      break;
    default:
      new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).to.have.property('length');
      itemsCount = obj.length;
  }

  this.assert(
    itemsCount == n,
    'expected #{this} to have a ' + descriptor + ' of #{exp} but got #{act}',
    'expected #{this} to not have a ' + descriptor + ' of #{act}',
    n,
    itemsCount
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('length', assertLength, assertLengthChain);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addChainableMethod('lengthOf', assertLength, assertLengthChain);

/**
 * ### .match(re[, msg])
 *
 * Asserts that the target matches the given regular expression `re`.
 *
 *     expect('foobar').to.match(/^foo/);
 *
 * Add `.not` earlier in the chain to negate `.match`.
 *
 *     expect('foobar').to.not.match(/taco/);
 *
 * `.match` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect('foobar').to.match(/taco/, 'nooo why fail??');
 *     expect('foobar', 'nooo why fail??').to.match(/taco/);
 *
 * The alias `.matches` can be used interchangeably with `.match`.
 *
 * @name match
 * @alias matches
 * @param {RegExp} re
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertMatch(re, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object');
  this.assert(
    re.exec(obj),
    'expected #{this} to match ' + re,
    'expected #{this} not to match ' + re
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('match', assertMatch);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('matches', assertMatch);

/**
 * ### .string(str[, msg])
 *
 * Asserts that the target string contains the given substring `str`.
 *
 *     expect('foobar').to.have.string('bar');
 *
 * Add `.not` earlier in the chain to negate `.string`.
 *
 *     expect('foobar').to.not.have.string('taco');
 *
 * `.string` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect('foobar').to.have.string('taco', 'nooo why fail??');
 *     expect('foobar', 'nooo why fail??').to.have.string('taco');
 *
 * @name string
 * @param {string} str
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('string', function (str, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi');
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).is.a('string');

  this.assert(
    ~obj.indexOf(str),
    'expected #{this} to contain ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(str),
    'expected #{this} to not contain ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(str)
  );
});

/**
 * ### .keys(key1[, key2[, ...]])
 *
 * Asserts that the target object, array, map, or set has the given keys. Only
 * the target's own inherited properties are included in the search.
 *
 * When the target is an object or array, keys can be provided as one or more
 * string arguments, a single array argument, or a single object argument. In
 * the latter case, only the keys in the given object matter; the values are
 * ignored.
 *
 *     expect({a: 1, b: 2}).to.have.all.keys('a', 'b');
 *     expect(['x', 'y']).to.have.all.keys(0, 1);
 *
 *     expect({a: 1, b: 2}).to.have.all.keys(['a', 'b']);
 *     expect(['x', 'y']).to.have.all.keys([0, 1]);
 *
 *     expect({a: 1, b: 2}).to.have.all.keys({a: 4, b: 5}); // ignore 4 and 5
 *     expect(['x', 'y']).to.have.all.keys({0: 4, 1: 5}); // ignore 4 and 5
 *
 * When the target is a map or set, each key must be provided as a separate
 * argument.
 *
 *     expect(new Map([['a', 1], ['b', 2]])).to.have.all.keys('a', 'b');
 *     expect(new Set(['a', 'b'])).to.have.all.keys('a', 'b');
 *
 * Because `.keys` does different things based on the target's type, it's
 * important to check the target's type before using `.keys`. See the `.a` doc
 * for info on testing a target's type.
 *
 *     expect({a: 1, b: 2}).to.be.an('object').that.has.all.keys('a', 'b');
 *
 * By default, strict (`===`) equality is used to compare keys of maps and
 * sets. Add `.deep` earlier in the chain to use deep equality instead. See
 * the `deep-eql` project page for info on the deep equality algorithm:
 * https://github.com/chaijs/deep-eql.
 *
 *     // Target set deeply (but not strictly) has key `{a: 1}`
 *     expect(new Set([{a: 1}])).to.have.all.deep.keys([{a: 1}]);
 *     expect(new Set([{a: 1}])).to.not.have.all.keys([{a: 1}]);
 *
 * By default, the target must have all of the given keys and no more. Add
 * `.any` earlier in the chain to only require that the target have at least
 * one of the given keys. Also, add `.not` earlier in the chain to negate
 * `.keys`. It's often best to add `.any` when negating `.keys`, and to use
 * `.all` when asserting `.keys` without negation.
 *
 * When negating `.keys`, `.any` is preferred because `.not.any.keys` asserts
 * exactly what's expected of the output, whereas `.not.all.keys` creates
 * uncertain expectations.
 *
 *     // Recommended; asserts that target doesn't have any of the given keys
 *     expect({a: 1, b: 2}).to.not.have.any.keys('c', 'd');
 *
 *     // Not recommended; asserts that target doesn't have all of the given
 *     // keys but may or may not have some of them
 *     expect({a: 1, b: 2}).to.not.have.all.keys('c', 'd');
 *
 * When asserting `.keys` without negation, `.all` is preferred because
 * `.all.keys` asserts exactly what's expected of the output, whereas
 * `.any.keys` creates uncertain expectations.
 *
 *     // Recommended; asserts that target has all the given keys
 *     expect({a: 1, b: 2}).to.have.all.keys('a', 'b');
 *
 *     // Not recommended; asserts that target has at least one of the given
 *     // keys but may or may not have more of them
 *     expect({a: 1, b: 2}).to.have.any.keys('a', 'b');
 *
 * Note that `.all` is used by default when neither `.all` nor `.any` appear
 * earlier in the chain. However, it's often best to add `.all` anyway because
 * it improves readability.
 *
 *     // Both assertions are identical
 *     expect({a: 1, b: 2}).to.have.all.keys('a', 'b'); // Recommended
 *     expect({a: 1, b: 2}).to.have.keys('a', 'b'); // Not recommended
 *
 * Add `.include` earlier in the chain to require that the target's keys be a
 * superset of the expected keys, rather than identical sets.
 *
 *     // Target object's keys are a superset of ['a', 'b'] but not identical
 *     expect({a: 1, b: 2, c: 3}).to.include.all.keys('a', 'b');
 *     expect({a: 1, b: 2, c: 3}).to.not.have.all.keys('a', 'b');
 *
 * However, if `.any` and `.include` are combined, only the `.any` takes
 * effect. The `.include` is ignored in this case.
 *
 *     // Both assertions are identical
 *     expect({a: 1}).to.have.any.keys('a', 'b');
 *     expect({a: 1}).to.include.any.keys('a', 'b');
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect({a: 1}, 'nooo why fail??').to.have.key('b');
 *
 * The alias `.key` can be used interchangeably with `.keys`.
 *
 * @name keys
 * @alias key
 * @param {...string | Array | object} keys
 * @namespace BDD
 * @public
 */
function assertKeys(keys) {
  let obj = flag(this, 'object'),
    objType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(obj),
    keysType = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.type(keys),
    ssfi = flag(this, 'ssfi'),
    isDeep = flag(this, 'deep'),
    str,
    deepStr = '',
    actual,
    ok = true,
    flagMsg = flag(this, 'message');

  flagMsg = flagMsg ? flagMsg + ': ' : '';
  let mixedArgsMsg =
    flagMsg +
    'when testing keys against an object or an array you must give a single Array|Object|String argument or multiple String arguments';

  if (objType === 'Map' || objType === 'Set') {
    deepStr = isDeep ? 'deeply ' : '';
    actual = [];

    // Map and Set '.keys' aren't supported in IE 11. Therefore, use .forEach.
    obj.forEach(function (val, key) {
      actual.push(key);
    });

    if (keysType !== 'Array') {
      keys = Array.prototype.slice.call(arguments);
    }
  } else {
    actual = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.getOwnEnumerableProperties(obj);

    switch (keysType) {
      case 'Array':
        if (arguments.length > 1) {
          throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(mixedArgsMsg, undefined, ssfi);
        }
        break;
      case 'Object':
        if (arguments.length > 1) {
          throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(mixedArgsMsg, undefined, ssfi);
        }
        keys = Object.keys(keys);
        break;
      default:
        keys = Array.prototype.slice.call(arguments);
    }

    // Only stringify non-Symbols because Symbols would become "Symbol()"
    keys = keys.map(function (val) {
      return typeof val === 'symbol' ? val : String(val);
    });
  }

  if (!keys.length) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(flagMsg + 'keys required', undefined, ssfi);
  }

  let len = keys.length,
    any = flag(this, 'any'),
    all = flag(this, 'all'),
    expected = keys,
    isEql = isDeep ? flag(this, 'eql') : (val1, val2) => val1 === val2;

  if (!any && !all) {
    all = true;
  }

  // Has any
  if (any) {
    ok = expected.some(function (expectedKey) {
      return actual.some(function (actualKey) {
        return isEql(expectedKey, actualKey);
      });
    });
  }

  // Has all
  if (all) {
    ok = expected.every(function (expectedKey) {
      return actual.some(function (actualKey) {
        return isEql(expectedKey, actualKey);
      });
    });

    if (!flag(this, 'contains')) {
      ok = ok && keys.length == actual.length;
    }
  }

  // Key string
  if (len > 1) {
    keys = keys.map(function (key) {
      return _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(key);
    });
    let last = keys.pop();
    if (all) {
      str = keys.join(', ') + ', and ' + last;
    }
    if (any) {
      str = keys.join(', ') + ', or ' + last;
    }
  } else {
    str = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(keys[0]);
  }

  // Form
  str = (len > 1 ? 'keys ' : 'key ') + str;

  // Have / include
  str = (flag(this, 'contains') ? 'contain ' : 'have ') + str;

  // Assertion
  this.assert(
    ok,
    'expected #{this} to ' + deepStr + str,
    'expected #{this} to not ' + deepStr + str,
    expected.slice(0).sort(_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.compareByInspect),
    actual.sort(_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.compareByInspect),
    true
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('keys', assertKeys);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('key', assertKeys);

/**
 * ### .throw([errorLike], [errMsgMatcher], [msg])
 *
 * When no arguments are provided, `.throw` invokes the target function and
 * asserts that an error is thrown.
 *
 *     var badFn = function () { throw new TypeError('Illegal salmon!'); };
 *     expect(badFn).to.throw();
 *
 * When one argument is provided, and it's an error constructor, `.throw`
 * invokes the target function and asserts that an error is thrown that's an
 * instance of that error constructor.
 *
 *     var badFn = function () { throw new TypeError('Illegal salmon!'); };
 *     expect(badFn).to.throw(TypeError);
 *
 * When one argument is provided, and it's an error instance, `.throw` invokes
 * the target function and asserts that an error is thrown that's strictly
 * (`===`) equal to that error instance.
 *
 *     var err = new TypeError('Illegal salmon!');
 *     var badFn = function () { throw err; };
 *
 *     expect(badFn).to.throw(err);
 *
 * When one argument is provided, and it's a string, `.throw` invokes the
 * target function and asserts that an error is thrown with a message that
 * contains that string.
 *
 *     var badFn = function () { throw new TypeError('Illegal salmon!'); };
 *     expect(badFn).to.throw('salmon');
 *
 * When one argument is provided, and it's a regular expression, `.throw`
 * invokes the target function and asserts that an error is thrown with a
 * message that matches that regular expression.
 *
 *     var badFn = function () { throw new TypeError('Illegal salmon!'); };
 *     expect(badFn).to.throw(/salmon/);
 *
 * When two arguments are provided, and the first is an error instance or
 * constructor, and the second is a string or regular expression, `.throw`
 * invokes the function and asserts that an error is thrown that fulfills both
 * conditions as described above.
 *
 *     var err = new TypeError('Illegal salmon!');
 *     var badFn = function () { throw err; };
 *
 *     expect(badFn).to.throw(TypeError, 'salmon');
 *     expect(badFn).to.throw(TypeError, /salmon/);
 *     expect(badFn).to.throw(err, 'salmon');
 *     expect(badFn).to.throw(err, /salmon/);
 *
 * Add `.not` earlier in the chain to negate `.throw`.
 *
 *     var goodFn = function () {};
 *     expect(goodFn).to.not.throw();
 *
 * However, it's dangerous to negate `.throw` when providing any arguments.
 * The problem is that it creates uncertain expectations by asserting that the
 * target either doesn't throw an error, or that it throws an error but of a
 * different type than the given type, or that it throws an error of the given
 * type but with a message that doesn't include the given string. It's often
 * best to identify the exact output that's expected, and then write an
 * assertion that only accepts that exact output.
 *
 * When the target isn't expected to throw an error, it's often best to assert
 * exactly that.
 *
 *     var goodFn = function () {};
 *
 *     expect(goodFn).to.not.throw(); // Recommended
 *     expect(goodFn).to.not.throw(ReferenceError, 'x'); // Not recommended
 *
 * When the target is expected to throw an error, it's often best to assert
 * that the error is of its expected type, and has a message that includes an
 * expected string, rather than asserting that it doesn't have one of many
 * unexpected types, and doesn't have a message that includes some string.
 *
 *     var badFn = function () { throw new TypeError('Illegal salmon!'); };
 *
 *     expect(badFn).to.throw(TypeError, 'salmon'); // Recommended
 *     expect(badFn).to.not.throw(ReferenceError, 'x'); // Not recommended
 *
 * `.throw` changes the target of any assertions that follow in the chain to
 * be the error object that's thrown.
 *
 *     var err = new TypeError('Illegal salmon!');
 *     err.code = 42;
 *     var badFn = function () { throw err; };
 *
 *     expect(badFn).to.throw(TypeError).with.property('code', 42);
 *
 * `.throw` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`. When not providing two arguments, always use
 * the second form.
 *
 *     var goodFn = function () {};
 *
 *     expect(goodFn).to.throw(TypeError, 'x', 'nooo why fail??');
 *     expect(goodFn, 'nooo why fail??').to.throw();
 *
 * Due to limitations in ES5, `.throw` may not always work as expected when
 * using a transpiler such as Babel or TypeScript. In particular, it may
 * produce unexpected results when subclassing the built-in `Error` object and
 * then passing the subclassed constructor to `.throw`. See your transpiler's
 * docs for details:
 *
 * - ([Babel](https://babeljs.io/docs/usage/caveats/#classes))
 * - ([TypeScript](https://github.com/Microsoft/TypeScript/wiki/Breaking-Changes#extending-built-ins-like-error-array-and-map-may-no-longer-work))
 *
 * Beware of some common mistakes when using the `throw` assertion. One common
 * mistake is to accidentally invoke the function yourself instead of letting
 * the `throw` assertion invoke the function for you. For example, when
 * testing if a function named `fn` throws, provide `fn` instead of `fn()` as
 * the target for the assertion.
 *
 *     expect(fn).to.throw();     // Good! Tests `fn` as desired
 *     expect(fn()).to.throw();   // Bad! Tests result of `fn()`, not `fn`
 *
 * If you need to assert that your function `fn` throws when passed certain
 * arguments, then wrap a call to `fn` inside of another function.
 *
 *     expect(function () { fn(42); }).to.throw();  // Function expression
 *     expect(() => fn(42)).to.throw();             // ES6 arrow function
 *
 * Another common mistake is to provide an object method (or any stand-alone
 * function that relies on `this`) as the target of the assertion. Doing so is
 * problematic because the `this` context will be lost when the function is
 * invoked by `.throw`; there's no way for it to know what `this` is supposed
 * to be. There are two ways around this problem. One solution is to wrap the
 * method or function call inside of another function. Another solution is to
 * use `bind`.
 *
 *     expect(function () { cat.meow(); }).to.throw();  // Function expression
 *     expect(() => cat.meow()).to.throw();             // ES6 arrow function
 *     expect(cat.meow.bind(cat)).to.throw();           // Bind
 *
 * Finally, it's worth mentioning that it's a best practice in JavaScript to
 * only throw `Error` and derivatives of `Error` such as `ReferenceError`,
 * `TypeError`, and user-defined objects that extend `Error`. No other type of
 * value will generate a stack trace when initialized. With that said, the
 * `throw` assertion does technically support any type of value being thrown,
 * not just `Error` and its derivatives.
 *
 * The aliases `.throws` and `.Throw` can be used interchangeably with
 * `.throw`.
 *
 * @name throw
 * @alias throws
 * @alias Throw
 * @param {Error} errorLike
 * @param {string | RegExp} errMsgMatcher error message
 * @param {string} msg _optional_
 * @see https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error#Error_types
 * @returns {void} error for chaining (null if no error)
 * @namespace BDD
 * @public
 */
function assertThrows(errorLike, errMsgMatcher, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    ssfi = flag(this, 'ssfi'),
    flagMsg = flag(this, 'message'),
    negate = flag(this, 'negate') || false;
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).is.a('function');

  if (_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isRegExp(errorLike) || typeof errorLike === 'string') {
    errMsgMatcher = errorLike;
    errorLike = null;
  }

  let caughtErr;
  let errorWasThrown = false;
  try {
    obj();
  } catch (err) {
    errorWasThrown = true;
    caughtErr = err;
  }

  // If we have the negate flag enabled and at least one valid argument it means we do expect an error
  // but we want it to match a given set of criteria
  let everyArgIsUndefined =
    errorLike === undefined && errMsgMatcher === undefined;

  // If we've got the negate flag enabled and both args, we should only fail if both aren't compatible
  // See Issue #551 and PR #683@GitHub
  let everyArgIsDefined = Boolean(errorLike && errMsgMatcher);
  let errorLikeFail = false;
  let errMsgMatcherFail = false;

  // Checking if error was thrown
  if (everyArgIsUndefined || (!everyArgIsUndefined && !negate)) {
    // We need this to display results correctly according to their types
    let errorLikeString = 'an error';
    if (errorLike instanceof Error) {
      errorLikeString = '#{exp}';
    } else if (errorLike) {
      errorLikeString = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.getConstructorName(errorLike);
    }

    let actual = caughtErr;
    if (caughtErr instanceof Error) {
      actual = caughtErr.toString();
    } else if (typeof caughtErr === 'string') {
      actual = caughtErr;
    } else if (
      caughtErr &&
      (typeof caughtErr === 'object' || typeof caughtErr === 'function')
    ) {
      try {
        actual = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.getConstructorName(caughtErr);
      } catch (_err) {
        // somehow wasn't a constructor, maybe we got a function thrown
        // or similar
      }
    }

    this.assert(
      errorWasThrown,
      'expected #{this} to throw ' + errorLikeString,
      'expected #{this} to not throw an error but #{act} was thrown',
      errorLike && errorLike.toString(),
      actual
    );
  }

  if (errorLike && caughtErr) {
    // We should compare instances only if `errorLike` is an instance of `Error`
    if (errorLike instanceof Error) {
      let isCompatibleInstance = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.compatibleInstance(
        caughtErr,
        errorLike
      );

      if (isCompatibleInstance === negate) {
        // These checks were created to ensure we won't fail too soon when we've got both args and a negate
        // See Issue #551 and PR #683@GitHub
        if (everyArgIsDefined && negate) {
          errorLikeFail = true;
        } else {
          this.assert(
            negate,
            'expected #{this} to throw #{exp} but #{act} was thrown',
            'expected #{this} to not throw #{exp}' +
              (caughtErr && !negate ? ' but #{act} was thrown' : ''),
            errorLike.toString(),
            caughtErr.toString()
          );
        }
      }
    }

    let isCompatibleConstructor = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.compatibleConstructor(
      caughtErr,
      errorLike
    );
    if (isCompatibleConstructor === negate) {
      if (everyArgIsDefined && negate) {
        errorLikeFail = true;
      } else {
        this.assert(
          negate,
          'expected #{this} to throw #{exp} but #{act} was thrown',
          'expected #{this} to not throw #{exp}' +
            (caughtErr ? ' but #{act} was thrown' : ''),
          errorLike instanceof Error
            ? errorLike.toString()
            : errorLike && _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.getConstructorName(errorLike),
          caughtErr instanceof Error
            ? caughtErr.toString()
            : caughtErr && _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.getConstructorName(caughtErr)
        );
      }
    }
  }

  if (caughtErr && errMsgMatcher !== undefined && errMsgMatcher !== null) {
    // Here we check compatible messages
    let placeholder = 'including';
    if (_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.isRegExp(errMsgMatcher)) {
      placeholder = 'matching';
    }

    let isCompatibleMessage = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.compatibleMessage(
      caughtErr,
      errMsgMatcher
    );
    if (isCompatibleMessage === negate) {
      if (everyArgIsDefined && negate) {
        errMsgMatcherFail = true;
      } else {
        this.assert(
          negate,
          'expected #{this} to throw error ' +
            placeholder +
            ' #{exp} but got #{act}',
          'expected #{this} to throw error not ' + placeholder + ' #{exp}',
          errMsgMatcher,
          _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.getMessage(caughtErr)
        );
      }
    }
  }

  // If both assertions failed and both should've matched we throw an error
  if (errorLikeFail && errMsgMatcherFail) {
    this.assert(
      negate,
      'expected #{this} to throw #{exp} but #{act} was thrown',
      'expected #{this} to not throw #{exp}' +
        (caughtErr ? ' but #{act} was thrown' : ''),
      errorLike instanceof Error
        ? errorLike.toString()
        : errorLike && _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.getConstructorName(errorLike),
      caughtErr instanceof Error
        ? caughtErr.toString()
        : caughtErr && _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.checkError.getConstructorName(caughtErr)
    );
  }

  flag(this, 'object', caughtErr);
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('throw', assertThrows);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('throws', assertThrows);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('Throw', assertThrows);

/**
 * ### .respondTo(method[, msg])
 *
 * When the target is a non-function object, `.respondTo` asserts that the
 * target has a method with the given name `method`. The method can be own or
 * inherited, and it can be enumerable or non-enumerable.
 *
 *     function Cat () {}
 *     Cat.prototype.meow = function () {};
 *
 *     expect(new Cat()).to.respondTo('meow');
 *
 * When the target is a function, `.respondTo` asserts that the target's
 * `prototype` property has a method with the given name `method`. Again, the
 * method can be own or inherited, and it can be enumerable or non-enumerable.
 *
 *     function Cat () {}
 *     Cat.prototype.meow = function () {};
 *
 *     expect(Cat).to.respondTo('meow');
 *
 * Add `.itself` earlier in the chain to force `.respondTo` to treat the
 * target as a non-function object, even if it's a function. Thus, it asserts
 * that the target has a method with the given name `method`, rather than
 * asserting that the target's `prototype` property has a method with the
 * given name `method`.
 *
 *     function Cat () {}
 *     Cat.prototype.meow = function () {};
 *     Cat.hiss = function () {};
 *
 *     expect(Cat).itself.to.respondTo('hiss').but.not.respondTo('meow');
 *
 * When not adding `.itself`, it's important to check the target's type before
 * using `.respondTo`. See the `.a` doc for info on checking a target's type.
 *
 *     function Cat () {}
 *     Cat.prototype.meow = function () {};
 *
 *     expect(new Cat()).to.be.an('object').that.respondsTo('meow');
 *
 * Add `.not` earlier in the chain to negate `.respondTo`.
 *
 *     function Dog () {}
 *     Dog.prototype.bark = function () {};
 *
 *     expect(new Dog()).to.not.respondTo('meow');
 *
 * `.respondTo` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect({}).to.respondTo('meow', 'nooo why fail??');
 *     expect({}, 'nooo why fail??').to.respondTo('meow');
 *
 * The alias `.respondsTo` can be used interchangeably with `.respondTo`.
 *
 * @name respondTo
 * @alias respondsTo
 * @param {string} method
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function respondTo(method, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    itself = flag(this, 'itself'),
    context =
      'function' === typeof obj && !itself
        ? obj.prototype[method]
        : obj[method];

  this.assert(
    'function' === typeof context,
    'expected #{this} to respond to ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(method),
    'expected #{this} to not respond to ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect(method)
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('respondTo', respondTo);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('respondsTo', respondTo);

/**
 * ### .itself
 *
 * Forces all `.respondTo` assertions that follow in the chain to behave as if
 * the target is a non-function object, even if it's a function. Thus, it
 * causes `.respondTo` to assert that the target has a method with the given
 * name, rather than asserting that the target's `prototype` property has a
 * method with the given name.
 *
 *     function Cat () {}
 *     Cat.prototype.meow = function () {};
 *     Cat.hiss = function () {};
 *
 *     expect(Cat).itself.to.respondTo('hiss').but.not.respondTo('meow');
 *
 * @name itself
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('itself', function () {
  flag(this, 'itself', true);
});

/**
 * ### .satisfy(matcher[, msg])
 *
 * Invokes the given `matcher` function with the target being passed as the
 * first argument, and asserts that the value returned is truthy.
 *
 *     expect(1).to.satisfy(function(num) {
 *         return num > 0;
 *     });
 *
 * Add `.not` earlier in the chain to negate `.satisfy`.
 *
 *     expect(1).to.not.satisfy(function(num) {
 *         return num > 2;
 *     });
 *
 * `.satisfy` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect(1).to.satisfy(function(num) {
 *         return num > 2;
 *     }, 'nooo why fail??');
 *
 *     expect(1, 'nooo why fail??').to.satisfy(function(num) {
 *         return num > 2;
 *     });
 *
 * The alias `.satisfies` can be used interchangeably with `.satisfy`.
 *
 * @name satisfy
 * @alias satisfies
 * @param {Function} matcher
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function satisfy(matcher, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object');
  let result = matcher(obj);
  this.assert(
    result,
    'expected #{this} to satisfy ' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.objDisplay(matcher),
    'expected #{this} to not satisfy' + _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.objDisplay(matcher),
    flag(this, 'negate') ? false : true,
    result
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('satisfy', satisfy);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('satisfies', satisfy);

/**
 * ### .closeTo(expected, delta[, msg])
 *
 * Asserts that the target is a number that's within a given +/- `delta` range
 * of the given number `expected`. However, it's often best to assert that the
 * target is equal to its expected value.
 *
 *     // Recommended
 *     expect(1.5).to.equal(1.5);
 *
 *     // Not recommended
 *     expect(1.5).to.be.closeTo(1, 0.5);
 *     expect(1.5).to.be.closeTo(2, 0.5);
 *     expect(1.5).to.be.closeTo(1, 1);
 *
 * Add `.not` earlier in the chain to negate `.closeTo`.
 *
 *     expect(1.5).to.equal(1.5); // Recommended
 *     expect(1.5).to.not.be.closeTo(3, 1); // Not recommended
 *
 * `.closeTo` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect(1.5).to.be.closeTo(3, 1, 'nooo why fail??');
 *     expect(1.5, 'nooo why fail??').to.be.closeTo(3, 1);
 *
 * The alias `.approximately` can be used interchangeably with `.closeTo`.
 *
 * @name closeTo
 * @alias approximately
 * @param {number} expected
 * @param {number} delta
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function closeTo(expected, delta, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi');

  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).is.numeric;
  let message = 'A `delta` value is required for `closeTo`';
  if (delta == undefined) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      flagMsg ? `${flagMsg}: ${message}` : message,
      undefined,
      ssfi
    );
  }
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(delta, flagMsg, ssfi, true).is.numeric;
  message = 'A `expected` value is required for `closeTo`';
  if (expected == undefined) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      flagMsg ? `${flagMsg}: ${message}` : message,
      undefined,
      ssfi
    );
  }
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(expected, flagMsg, ssfi, true).is.numeric;

  const abs = (x) => (x < 0n ? -x : x);

  // Used to round floating point number precision arithmetics
  // See: https://stackoverflow.com/a/3644302
  const strip = (number) => parseFloat(parseFloat(number).toPrecision(12));

  this.assert(
    strip(abs(obj - expected)) <= delta,
    'expected #{this} to be close to ' + expected + ' +/- ' + delta,
    'expected #{this} not to be close to ' + expected + ' +/- ' + delta
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('closeTo', closeTo);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('approximately', closeTo);

/**
 * @param {unknown} _subset
 * @param {unknown} _superset
 * @param {unknown} cmp
 * @param {unknown} contains
 * @param {unknown} ordered
 * @returns {boolean}
 */
function isSubsetOf(_subset, _superset, cmp, contains, ordered) {
  let superset = Array.from(_superset);
  let subset = Array.from(_subset);
  if (!contains) {
    if (subset.length !== superset.length) return false;
    superset = superset.slice();
  }

  return subset.every(function (elem, idx) {
    if (ordered) return cmp ? cmp(elem, superset[idx]) : elem === superset[idx];

    if (!cmp) {
      let matchIdx = superset.indexOf(elem);
      if (matchIdx === -1) return false;

      // Remove match from superset so not counted twice if duplicate in subset.
      if (!contains) superset.splice(matchIdx, 1);
      return true;
    }

    return superset.some(function (elem2, matchIdx) {
      if (!cmp(elem, elem2)) return false;

      // Remove match from superset so not counted twice if duplicate in subset.
      if (!contains) superset.splice(matchIdx, 1);
      return true;
    });
  });
}

/**
 * ### .members(set[, msg])
 *
 * Asserts that the target array has the same members as the given array
 * `set`.
 *
 *     expect([1, 2, 3]).to.have.members([2, 1, 3]);
 *     expect([1, 2, 2]).to.have.members([2, 1, 2]);
 *
 * By default, members are compared using strict (`===`) equality. Add `.deep`
 * earlier in the chain to use deep equality instead. See the `deep-eql`
 * project page for info on the deep equality algorithm:
 * https://github.com/chaijs/deep-eql.
 *
 *     // Target array deeply (but not strictly) has member `{a: 1}`
 *     expect([{a: 1}]).to.have.deep.members([{a: 1}]);
 *     expect([{a: 1}]).to.not.have.members([{a: 1}]);
 *
 * By default, order doesn't matter. Add `.ordered` earlier in the chain to
 * require that members appear in the same order.
 *
 *     expect([1, 2, 3]).to.have.ordered.members([1, 2, 3]);
 *     expect([1, 2, 3]).to.have.members([2, 1, 3])
 *       .but.not.ordered.members([2, 1, 3]);
 *
 * By default, both arrays must be the same size. Add `.include` earlier in
 * the chain to require that the target's members be a superset of the
 * expected members. Note that duplicates are ignored in the subset when
 * `.include` is added.
 *
 *     // Target array is a superset of [1, 2] but not identical
 *     expect([1, 2, 3]).to.include.members([1, 2]);
 *     expect([1, 2, 3]).to.not.have.members([1, 2]);
 *
 *     // Duplicates in the subset are ignored
 *     expect([1, 2, 3]).to.include.members([1, 2, 2, 2]);
 *
 * `.deep`, `.ordered`, and `.include` can all be combined. However, if
 * `.include` and `.ordered` are combined, the ordering begins at the start of
 * both arrays.
 *
 *     expect([{a: 1}, {b: 2}, {c: 3}])
 *       .to.include.deep.ordered.members([{a: 1}, {b: 2}])
 *       .but.not.include.deep.ordered.members([{b: 2}, {c: 3}]);
 *
 * Add `.not` earlier in the chain to negate `.members`. However, it's
 * dangerous to do so. The problem is that it creates uncertain expectations
 * by asserting that the target array doesn't have all of the same members as
 * the given array `set` but may or may not have some of them. It's often best
 * to identify the exact output that's expected, and then write an assertion
 * that only accepts that exact output.
 *
 *     expect([1, 2]).to.not.include(3).and.not.include(4); // Recommended
 *     expect([1, 2]).to.not.have.members([3, 4]); // Not recommended
 *
 * `.members` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`.
 *
 *     expect([1, 2]).to.have.members([1, 2, 3], 'nooo why fail??');
 *     expect([1, 2], 'nooo why fail??').to.have.members([1, 2, 3]);
 *
 * @name members
 * @param {Array} set
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('members', function (subset, msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object'),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi');

  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(obj, flagMsg, ssfi, true).to.be.iterable;
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(subset, flagMsg, ssfi, true).to.be.iterable;

  let contains = flag(this, 'contains');
  let ordered = flag(this, 'ordered');

  let subject, failMsg, failNegateMsg;

  if (contains) {
    subject = ordered ? 'an ordered superset' : 'a superset';
    failMsg = 'expected #{this} to be ' + subject + ' of #{exp}';
    failNegateMsg = 'expected #{this} to not be ' + subject + ' of #{exp}';
  } else {
    subject = ordered ? 'ordered members' : 'members';
    failMsg = 'expected #{this} to have the same ' + subject + ' as #{exp}';
    failNegateMsg =
      'expected #{this} to not have the same ' + subject + ' as #{exp}';
  }

  let cmp = flag(this, 'deep') ? flag(this, 'eql') : undefined;

  this.assert(
    isSubsetOf(subset, obj, cmp, contains, ordered),
    failMsg,
    failNegateMsg,
    subset,
    obj,
    true
  );
});

/**
 * ### .iterable
 *
 * Asserts that the target is an iterable, which means that it has a iterator.
 *
 *     expect([1, 2]).to.be.iterable;
 *     expect("foobar").to.be.iterable;
 *
 * Add `.not` earlier in the chain to negate `.iterable`.
 *
 *     expect(1).to.not.be.iterable;
 *     expect(true).to.not.be.iterable;
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(1, 'nooo why fail??').to.be.iterable;
 *
 * @name iterable
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('iterable', function (msg) {
  if (msg) flag(this, 'message', msg);
  let obj = flag(this, 'object');

  this.assert(
    obj != undefined && obj[Symbol.iterator],
    'expected #{this} to be an iterable',
    'expected #{this} to not be an iterable',
    obj
  );
});

/**
 * ### .oneOf(list[, msg])
 *
 * Asserts that the target is a member of the given array `list`. However,
 * it's often best to assert that the target is equal to its expected value.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.be.oneOf([1, 2, 3]); // Not recommended
 *
 * Comparisons are performed using strict (`===`) equality.
 *
 * Add `.not` earlier in the chain to negate `.oneOf`.
 *
 *     expect(1).to.equal(1); // Recommended
 *     expect(1).to.not.be.oneOf([2, 3, 4]); // Not recommended
 *
 * It can also be chained with `.contain` or `.include`, which will work with
 * both arrays and strings:
 *
 *     expect('Today is sunny').to.contain.oneOf(['sunny', 'cloudy'])
 *     expect('Today is rainy').to.not.contain.oneOf(['sunny', 'cloudy'])
 *     expect([1,2,3]).to.contain.oneOf([3,4,5])
 *     expect([1,2,3]).to.not.contain.oneOf([4,5,6])
 *
 * `.oneOf` accepts an optional `msg` argument which is a custom error message
 * to show when the assertion fails. The message can also be given as the
 * second argument to `expect`.
 *
 *     expect(1).to.be.oneOf([2, 3, 4], 'nooo why fail??');
 *     expect(1, 'nooo why fail??').to.be.oneOf([2, 3, 4]);
 *
 * @name oneOf
 * @param {Array<*>} list
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function oneOf(list, msg) {
  if (msg) flag(this, 'message', msg);
  let expected = flag(this, 'object'),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi'),
    contains = flag(this, 'contains'),
    isDeep = flag(this, 'deep'),
    eql = flag(this, 'eql');
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(list, flagMsg, ssfi, true).to.be.an('array');

  if (contains) {
    this.assert(
      list.some(function (possibility) {
        return expected.indexOf(possibility) > -1;
      }),
      'expected #{this} to contain one of #{exp}',
      'expected #{this} to not contain one of #{exp}',
      list,
      expected
    );
  } else {
    if (isDeep) {
      this.assert(
        list.some(function (possibility) {
          return eql(expected, possibility);
        }),
        'expected #{this} to deeply equal one of #{exp}',
        'expected #{this} to deeply equal one of #{exp}',
        list,
        expected
      );
    } else {
      this.assert(
        list.indexOf(expected) > -1,
        'expected #{this} to be one of #{exp}',
        'expected #{this} to not be one of #{exp}',
        list,
        expected
      );
    }
  }
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('oneOf', oneOf);

/**
 * ### .change(subject[, prop[, msg]])
 *
 * When one argument is provided, `.change` asserts that the given function
 * `subject` returns a different value when it's invoked before the target
 * function compared to when it's invoked afterward. However, it's often best
 * to assert that `subject` is equal to its expected value.
 *
 *     var dots = ''
 *     , addDot = function () { dots += '.'; }
 *     , getDots = function () { return dots; };
 *
 *     // Recommended
 *     expect(getDots()).to.equal('');
 *     addDot();
 *     expect(getDots()).to.equal('.');
 *
 *     // Not recommended
 *     expect(addDot).to.change(getDots);
 *
 * When two arguments are provided, `.change` asserts that the value of the
 * given object `subject`'s `prop` property is different before invoking the
 * target function compared to afterward.
 *
 *     var myObj = {dots: ''}
 *     , addDot = function () { myObj.dots += '.'; };
 *
 *     // Recommended
 *     expect(myObj).to.have.property('dots', '');
 *     addDot();
 *     expect(myObj).to.have.property('dots', '.');
 *
 *     // Not recommended
 *     expect(addDot).to.change(myObj, 'dots');
 *
 * Strict (`===`) equality is used to compare before and after values.
 *
 * Add `.not` earlier in the chain to negate `.change`.
 *
 *     var dots = ''
 *     , noop = function () {}
 *     , getDots = function () { return dots; };
 *
 *     expect(noop).to.not.change(getDots);
 *
 *     var myObj = {dots: ''}
 *     , noop = function () {};
 *
 *     expect(noop).to.not.change(myObj, 'dots');
 *
 * `.change` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`. When not providing two arguments, always
 * use the second form.
 *
 *     var myObj = {dots: ''}
 *     , addDot = function () { myObj.dots += '.'; };
 *
 *     expect(addDot).to.not.change(myObj, 'dots', 'nooo why fail??');
 *
 *     var dots = ''
 *     , addDot = function () { dots += '.'; }
 *     , getDots = function () { return dots; };
 *
 *     expect(addDot, 'nooo why fail??').to.not.change(getDots);
 *
 * `.change` also causes all `.by` assertions that follow in the chain to
 * assert how much a numeric subject was increased or decreased by. However,
 * it's dangerous to use `.change.by`. The problem is that it creates
 * uncertain expectations by asserting that the subject either increases by
 * the given delta, or that it decreases by the given delta. It's often best
 * to identify the exact output that's expected, and then write an assertion
 * that only accepts that exact output.
 *
 *     var myObj = {val: 1}
 *     , addTwo = function () { myObj.val += 2; }
 *     , subtractTwo = function () { myObj.val -= 2; };
 *
 *     expect(addTwo).to.increase(myObj, 'val').by(2); // Recommended
 *     expect(addTwo).to.change(myObj, 'val').by(2); // Not recommended
 *
 *     expect(subtractTwo).to.decrease(myObj, 'val').by(2); // Recommended
 *     expect(subtractTwo).to.change(myObj, 'val').by(2); // Not recommended
 *
 * The alias `.changes` can be used interchangeably with `.change`.
 *
 * @name change
 * @alias changes
 * @param {string} subject
 * @param {string} prop name _optional_
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertChanges(subject, prop, msg) {
  if (msg) flag(this, 'message', msg);
  let fn = flag(this, 'object'),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi');
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(fn, flagMsg, ssfi, true).is.a('function');

  let initial;
  if (!prop) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(subject, flagMsg, ssfi, true).is.a('function');
    initial = subject();
  } else {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(subject, flagMsg, ssfi, true).to.have.property(prop);
    initial = subject[prop];
  }

  fn();

  let final = prop === undefined || prop === null ? subject() : subject[prop];
  let msgObj = prop === undefined || prop === null ? initial : '.' + prop;

  // This gets flagged because of the .by(delta) assertion
  flag(this, 'deltaMsgObj', msgObj);
  flag(this, 'initialDeltaValue', initial);
  flag(this, 'finalDeltaValue', final);
  flag(this, 'deltaBehavior', 'change');
  flag(this, 'realDelta', final !== initial);

  this.assert(
    initial !== final,
    'expected ' + msgObj + ' to change',
    'expected ' + msgObj + ' to not change'
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('change', assertChanges);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('changes', assertChanges);

/**
 * ### .increase(subject[, prop[, msg]])
 *
 * When one argument is provided, `.increase` asserts that the given function
 * `subject` returns a greater number when it's invoked after invoking the
 * target function compared to when it's invoked beforehand. `.increase` also
 * causes all `.by` assertions that follow in the chain to assert how much
 * greater of a number is returned. It's often best to assert that the return
 * value increased by the expected amount, rather than asserting it increased
 * by any amount.
 *
 *     var val = 1
 *     , addTwo = function () { val += 2; }
 *     , getVal = function () { return val; };
 *
 *     expect(addTwo).to.increase(getVal).by(2); // Recommended
 *     expect(addTwo).to.increase(getVal); // Not recommended
 *
 * When two arguments are provided, `.increase` asserts that the value of the
 * given object `subject`'s `prop` property is greater after invoking the
 * target function compared to beforehand.
 *
 *     var myObj = {val: 1}
 *     , addTwo = function () { myObj.val += 2; };
 *
 *     expect(addTwo).to.increase(myObj, 'val').by(2); // Recommended
 *     expect(addTwo).to.increase(myObj, 'val'); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.increase`. However, it's
 * dangerous to do so. The problem is that it creates uncertain expectations
 * by asserting that the subject either decreases, or that it stays the same.
 * It's often best to identify the exact output that's expected, and then
 * write an assertion that only accepts that exact output.
 *
 * When the subject is expected to decrease, it's often best to assert that it
 * decreased by the expected amount.
 *
 *     var myObj = {val: 1}
 *     , subtractTwo = function () { myObj.val -= 2; };
 *
 *     expect(subtractTwo).to.decrease(myObj, 'val').by(2); // Recommended
 *     expect(subtractTwo).to.not.increase(myObj, 'val'); // Not recommended
 *
 * When the subject is expected to stay the same, it's often best to assert
 * exactly that.
 *
 *     var myObj = {val: 1}
 *     , noop = function () {};
 *
 *     expect(noop).to.not.change(myObj, 'val'); // Recommended
 *     expect(noop).to.not.increase(myObj, 'val'); // Not recommended
 *
 * `.increase` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`. When not providing two arguments, always
 * use the second form.
 *
 *     var myObj = {val: 1}
 *     , noop = function () {};
 *
 *     expect(noop).to.increase(myObj, 'val', 'nooo why fail??');
 *
 *     var val = 1
 *     , noop = function () {}
 *     , getVal = function () { return val; };
 *
 *     expect(noop, 'nooo why fail??').to.increase(getVal);
 *
 * The alias `.increases` can be used interchangeably with `.increase`.
 *
 * @name increase
 * @alias increases
 * @param {string | Function} subject
 * @param {string} prop name _optional_
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertIncreases(subject, prop, msg) {
  if (msg) flag(this, 'message', msg);
  let fn = flag(this, 'object'),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi');
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(fn, flagMsg, ssfi, true).is.a('function');

  let initial;
  if (!prop) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(subject, flagMsg, ssfi, true).is.a('function');
    initial = subject();
  } else {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(subject, flagMsg, ssfi, true).to.have.property(prop);
    initial = subject[prop];
  }

  // Make sure that the target is a number
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(initial, flagMsg, ssfi, true).is.a('number');

  fn();

  let final = prop === undefined || prop === null ? subject() : subject[prop];
  let msgObj = prop === undefined || prop === null ? initial : '.' + prop;

  flag(this, 'deltaMsgObj', msgObj);
  flag(this, 'initialDeltaValue', initial);
  flag(this, 'finalDeltaValue', final);
  flag(this, 'deltaBehavior', 'increase');
  flag(this, 'realDelta', final - initial);

  this.assert(
    final - initial > 0,
    'expected ' + msgObj + ' to increase',
    'expected ' + msgObj + ' to not increase'
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('increase', assertIncreases);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('increases', assertIncreases);

/**
 * ### .decrease(subject[, prop[, msg]])
 *
 * When one argument is provided, `.decrease` asserts that the given function
 * `subject` returns a lesser number when it's invoked after invoking the
 * target function compared to when it's invoked beforehand. `.decrease` also
 * causes all `.by` assertions that follow in the chain to assert how much
 * lesser of a number is returned. It's often best to assert that the return
 * value decreased by the expected amount, rather than asserting it decreased
 * by any amount.
 *
 *     var val = 1
 *       , subtractTwo = function () { val -= 2; }
 *       , getVal = function () { return val; };
 *
 *     expect(subtractTwo).to.decrease(getVal).by(2); // Recommended
 *     expect(subtractTwo).to.decrease(getVal); // Not recommended
 *
 * When two arguments are provided, `.decrease` asserts that the value of the
 * given object `subject`'s `prop` property is lesser after invoking the
 * target function compared to beforehand.
 *
 *     var myObj = {val: 1}
 *       , subtractTwo = function () { myObj.val -= 2; };
 *
 *     expect(subtractTwo).to.decrease(myObj, 'val').by(2); // Recommended
 *     expect(subtractTwo).to.decrease(myObj, 'val'); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.decrease`. However, it's
 * dangerous to do so. The problem is that it creates uncertain expectations
 * by asserting that the subject either increases, or that it stays the same.
 * It's often best to identify the exact output that's expected, and then
 * write an assertion that only accepts that exact output.
 *
 * When the subject is expected to increase, it's often best to assert that it
 * increased by the expected amount.
 *
 *     var myObj = {val: 1}
 *       , addTwo = function () { myObj.val += 2; };
 *
 *     expect(addTwo).to.increase(myObj, 'val').by(2); // Recommended
 *     expect(addTwo).to.not.decrease(myObj, 'val'); // Not recommended
 *
 * When the subject is expected to stay the same, it's often best to assert
 * exactly that.
 *
 *     var myObj = {val: 1}
 *       , noop = function () {};
 *
 *     expect(noop).to.not.change(myObj, 'val'); // Recommended
 *     expect(noop).to.not.decrease(myObj, 'val'); // Not recommended
 *
 * `.decrease` accepts an optional `msg` argument which is a custom error
 * message to show when the assertion fails. The message can also be given as
 * the second argument to `expect`. When not providing two arguments, always
 * use the second form.
 *
 *     var myObj = {val: 1}
 *       , noop = function () {};
 *
 *     expect(noop).to.decrease(myObj, 'val', 'nooo why fail??');
 *
 *     var val = 1
 *       , noop = function () {}
 *       , getVal = function () { return val; };
 *
 *     expect(noop, 'nooo why fail??').to.decrease(getVal);
 *
 * The alias `.decreases` can be used interchangeably with `.decrease`.
 *
 * @name decrease
 * @alias decreases
 * @param {string | Function} subject
 * @param {string} prop name _optional_
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertDecreases(subject, prop, msg) {
  if (msg) flag(this, 'message', msg);
  let fn = flag(this, 'object'),
    flagMsg = flag(this, 'message'),
    ssfi = flag(this, 'ssfi');
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(fn, flagMsg, ssfi, true).is.a('function');

  let initial;
  if (!prop) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(subject, flagMsg, ssfi, true).is.a('function');
    initial = subject();
  } else {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(subject, flagMsg, ssfi, true).to.have.property(prop);
    initial = subject[prop];
  }

  // Make sure that the target is a number
  new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(initial, flagMsg, ssfi, true).is.a('number');

  fn();

  let final = prop === undefined || prop === null ? subject() : subject[prop];
  let msgObj = prop === undefined || prop === null ? initial : '.' + prop;

  flag(this, 'deltaMsgObj', msgObj);
  flag(this, 'initialDeltaValue', initial);
  flag(this, 'finalDeltaValue', final);
  flag(this, 'deltaBehavior', 'decrease');
  flag(this, 'realDelta', initial - final);

  this.assert(
    final - initial < 0,
    'expected ' + msgObj + ' to decrease',
    'expected ' + msgObj + ' to not decrease'
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('decrease', assertDecreases);
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('decreases', assertDecreases);

/**
 * ### .by(delta[, msg])
 *
 * When following an `.increase` assertion in the chain, `.by` asserts that
 * the subject of the `.increase` assertion increased by the given `delta`.
 *
 *     var myObj = {val: 1}
 *       , addTwo = function () { myObj.val += 2; };
 *
 *     expect(addTwo).to.increase(myObj, 'val').by(2);
 *
 * When following a `.decrease` assertion in the chain, `.by` asserts that the
 * subject of the `.decrease` assertion decreased by the given `delta`.
 *
 *     var myObj = {val: 1}
 *       , subtractTwo = function () { myObj.val -= 2; };
 *
 *     expect(subtractTwo).to.decrease(myObj, 'val').by(2);
 *
 * When following a `.change` assertion in the chain, `.by` asserts that the
 * subject of the `.change` assertion either increased or decreased by the
 * given `delta`. However, it's dangerous to use `.change.by`. The problem is
 * that it creates uncertain expectations. It's often best to identify the
 * exact output that's expected, and then write an assertion that only accepts
 * that exact output.
 *
 *     var myObj = {val: 1}
 *       , addTwo = function () { myObj.val += 2; }
 *       , subtractTwo = function () { myObj.val -= 2; };
 *
 *     expect(addTwo).to.increase(myObj, 'val').by(2); // Recommended
 *     expect(addTwo).to.change(myObj, 'val').by(2); // Not recommended
 *
 *     expect(subtractTwo).to.decrease(myObj, 'val').by(2); // Recommended
 *     expect(subtractTwo).to.change(myObj, 'val').by(2); // Not recommended
 *
 * Add `.not` earlier in the chain to negate `.by`. However, it's often best
 * to assert that the subject changed by its expected delta, rather than
 * asserting that it didn't change by one of countless unexpected deltas.
 *
 *     var myObj = {val: 1}
 *       , addTwo = function () { myObj.val += 2; };
 *
 *     // Recommended
 *     expect(addTwo).to.increase(myObj, 'val').by(2);
 *
 *     // Not recommended
 *     expect(addTwo).to.increase(myObj, 'val').but.not.by(3);
 *
 * `.by` accepts an optional `msg` argument which is a custom error message to
 * show when the assertion fails. The message can also be given as the second
 * argument to `expect`.
 *
 *     var myObj = {val: 1}
 *       , addTwo = function () { myObj.val += 2; };
 *
 *     expect(addTwo).to.increase(myObj, 'val').by(3, 'nooo why fail??');
 *     expect(addTwo, 'nooo why fail??').to.increase(myObj, 'val').by(3);
 *
 * @name by
 * @param {number} delta
 * @param {string} msg _optional_
 * @namespace BDD
 * @public
 */
function assertDelta(delta, msg) {
  if (msg) flag(this, 'message', msg);

  let msgObj = flag(this, 'deltaMsgObj');
  let initial = flag(this, 'initialDeltaValue');
  let final = flag(this, 'finalDeltaValue');
  let behavior = flag(this, 'deltaBehavior');
  let realDelta = flag(this, 'realDelta');

  let expression;
  if (behavior === 'change') {
    expression = Math.abs(final - initial) === Math.abs(delta);
  } else {
    expression = realDelta === Math.abs(delta);
  }

  this.assert(
    expression,
    'expected ' + msgObj + ' to ' + behavior + ' by ' + delta,
    'expected ' + msgObj + ' to not ' + behavior + ' by ' + delta
  );
}

_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('by', assertDelta);

/**
 * ### .extensible
 *
 * Asserts that the target is extensible, which means that new properties can
 * be added to it. Primitives are never extensible.
 *
 *     expect({a: 1}).to.be.extensible;
 *
 * Add `.not` earlier in the chain to negate `.extensible`.
 *
 *     var nonExtensibleObject = Object.preventExtensions({})
 *     , sealedObject = Object.seal({})
 *     , frozenObject = Object.freeze({});
 *
 *     expect(nonExtensibleObject).to.not.be.extensible;
 *     expect(sealedObject).to.not.be.extensible;
 *     expect(frozenObject).to.not.be.extensible;
 *     expect(1).to.not.be.extensible;
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect(1, 'nooo why fail??').to.be.extensible;
 *
 * @name extensible
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('extensible', function () {
  let obj = flag(this, 'object');

  // In ES5, if the argument to this method is a primitive, then it will cause a TypeError.
  // In ES6, a non-object argument will be treated as if it was a non-extensible ordinary object, simply return false.
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/isExtensible
  // The following provides ES6 behavior for ES5 environments.

  let isExtensible = obj === Object(obj) && Object.isExtensible(obj);

  this.assert(
    isExtensible,
    'expected #{this} to be extensible',
    'expected #{this} to not be extensible'
  );
});

/**
 * ### .sealed
 *
 * Asserts that the target is sealed, which means that new properties can't be
 * added to it, and its existing properties can't be reconfigured or deleted.
 * However, it's possible that its existing properties can still be reassigned
 * to different values. Primitives are always sealed.
 *
 *     var sealedObject = Object.seal({});
 *     var frozenObject = Object.freeze({});
 *
 *     expect(sealedObject).to.be.sealed;
 *     expect(frozenObject).to.be.sealed;
 *     expect(1).to.be.sealed;
 *
 * Add `.not` earlier in the chain to negate `.sealed`.
 *
 *     expect({a: 1}).to.not.be.sealed;
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect({a: 1}, 'nooo why fail??').to.be.sealed;
 *
 * @name sealed
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('sealed', function () {
  let obj = flag(this, 'object');

  // In ES5, if the argument to this method is a primitive, then it will cause a TypeError.
  // In ES6, a non-object argument will be treated as if it was a sealed ordinary object, simply return true.
  // See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/isSealed
  // The following provides ES6 behavior for ES5 environments.

  let isSealed = obj === Object(obj) ? Object.isSealed(obj) : true;

  this.assert(
    isSealed,
    'expected #{this} to be sealed',
    'expected #{this} to not be sealed'
  );
});

/**
 * ### .frozen
 *
 * Asserts that the target is frozen, which means that new properties can't be
 * added to it, and its existing properties can't be reassigned to different
 * values, reconfigured, or deleted. Primitives are always frozen.
 *
 *     var frozenObject = Object.freeze({});
 *
 *     expect(frozenObject).to.be.frozen;
 *     expect(1).to.be.frozen;
 *
 * Add `.not` earlier in the chain to negate `.frozen`.
 *
 *     expect({a: 1}).to.not.be.frozen;
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect({a: 1}, 'nooo why fail??').to.be.frozen;
 *
 * @name frozen
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('frozen', function () {
  let obj = flag(this, 'object');

  // In ES5, if the argument to this method is a primitive, then it will cause a TypeError.
  // In ES6, a non-object argument will be treated as if it was a frozen ordinary object, simply return true.
  // See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/isFrozen
  // The following provides ES6 behavior for ES5 environments.

  let isFrozen = obj === Object(obj) ? Object.isFrozen(obj) : true;

  this.assert(
    isFrozen,
    'expected #{this} to be frozen',
    'expected #{this} to not be frozen'
  );
});

/**
 * ### .finite
 *
 * Asserts that the target is a number, and isn't `NaN` or positive/negative
 * `Infinity`.
 *
 *     expect(1).to.be.finite;
 *
 * Add `.not` earlier in the chain to negate `.finite`. However, it's
 * dangerous to do so. The problem is that it creates uncertain expectations
 * by asserting that the subject either isn't a number, or that it's `NaN`, or
 * that it's positive `Infinity`, or that it's negative `Infinity`. It's often
 * best to identify the exact output that's expected, and then write an
 * assertion that only accepts that exact output.
 *
 * When the target isn't expected to be a number, it's often best to assert
 * that it's the expected type, rather than asserting that it isn't one of
 * many unexpected types.
 *
 *     expect('foo').to.be.a('string'); // Recommended
 *     expect('foo').to.not.be.finite; // Not recommended
 *
 * When the target is expected to be `NaN`, it's often best to assert exactly
 * that.
 *
 *     expect(NaN).to.be.NaN; // Recommended
 *     expect(NaN).to.not.be.finite; // Not recommended
 *
 * When the target is expected to be positive infinity, it's often best to
 * assert exactly that.
 *
 *     expect(Infinity).to.equal(Infinity); // Recommended
 *     expect(Infinity).to.not.be.finite; // Not recommended
 *
 * When the target is expected to be negative infinity, it's often best to
 * assert exactly that.
 *
 *     expect(-Infinity).to.equal(-Infinity); // Recommended
 *     expect(-Infinity).to.not.be.finite; // Not recommended
 *
 * A custom error message can be given as the second argument to `expect`.
 *
 *     expect('foo', 'nooo why fail??').to.be.finite;
 *
 * @name finite
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addProperty('finite', function (_msg) {
  let obj = flag(this, 'object');

  this.assert(
    typeof obj === 'number' && isFinite(obj),
    'expected #{this} to be a finite number',
    'expected #{this} to not be a finite number'
  );
});

/**
 * A subset-aware compare function
 *
 * @param {unknown} expected
 * @param {unknown} actual
 * @returns {boolean}
 */
function compareSubset(expected, actual) {
  if (expected === actual) {
    return true;
  }
  if (typeof actual !== typeof expected) {
    return false;
  }
  if (typeof expected !== 'object' || expected === null) {
    return expected === actual;
  }
  if (!actual) {
    return false;
  }

  if (Array.isArray(expected)) {
    if (!Array.isArray(actual)) {
      return false;
    }
    return expected.every(function (exp) {
      return actual.some(function (act) {
        return compareSubset(exp, act);
      });
    });
  }

  if (expected instanceof Date) {
    if (actual instanceof Date) {
      return expected.getTime() === actual.getTime();
    } else {
      return false;
    }
  }

  return Object.keys(expected).every(function (key) {
    let expectedValue = expected[key];
    let actualValue = actual[key];
    if (
      typeof expectedValue === 'object' &&
      expectedValue !== null &&
      actualValue !== null
    ) {
      return compareSubset(expectedValue, actualValue);
    }
    if (typeof expectedValue === 'function') {
      return expectedValue(actualValue);
    }
    return actualValue === expectedValue;
  });
}

/**
 * ### .containSubset
 *
 * Asserts that the target primitive/object/array structure deeply contains all provided fields
 * at the same key/depth as the provided structure.
 *
 * When comparing arrays, the target must contain the subset of at least one of each object/value in the subset array.
 * Order does not matter.
 *
 *     expect({name: {first: "John", last: "Smith"}}).to.containSubset({name: {first: "John"}});
 *
 * Add `.not` earlier in the chain to negate the assertion. This will cause the assertion to fail
 * only if the target DOES contains the provided data at the expected keys/depths.
 *
 * @name containSubset
 * @namespace BDD
 * @public
 */
_assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion.addMethod('containSubset', function (expected) {
  const actual = _utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag(this, 'object');
  const showDiff = _config_js__WEBPACK_IMPORTED_MODULE_3__.config.showDiff;

  this.assert(
    compareSubset(expected, actual),
    'expected #{act} to contain subset #{exp}',
    'expected #{act} to not contain subset #{exp}',
    expected,
    actual,
    showDiff
  );
});


/***/ }),

/***/ "./node_modules/chai/lib/chai/interface/assert.js":
/*!********************************************************!*\
  !*** ./node_modules/chai/lib/chai/interface/assert.js ***!
  \********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   assert: () => (/* binding */ assert)
/* harmony export */ });
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../../../index.js */ "./node_modules/chai/index.js");
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var _utils_index_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../utils/index.js */ "./node_modules/chai/lib/chai/utils/index.js");
/* harmony import */ var assertion_error__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! assertion-error */ "./node_modules/assertion-error/index.js");
/*!
 * chai
 * Copyright(c) 2011-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */






/**
 * ### assert(expression, message)
 *
 * Write your own test expressions.
 *
 *     assert('foo' !== 'bar', 'foo is not bar');
 *     assert(Array.isArray([]), 'empty arrays are arrays');
 *
 * @param {unknown} express - expression to test for truthiness
 * @param {string} errmsg - message to display on error
 * @name assert
 * @namespace Assert
 * @public
 */
function assert(express, errmsg) {
  let test = new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(null, null, _index_js__WEBPACK_IMPORTED_MODULE_0__.assert, true);
  test.assert(express, errmsg, '[ negation message unavailable ]');
}

/**
 * ### .fail([message])
 * ### .fail(actual, expected, [message], [operator])
 *
 * Throw a failure. Node.js `assert` module-compatible.
 *
 *     assert.fail();
 *     assert.fail("custom error message");
 *     assert.fail(1, 2);
 *     assert.fail(1, 2, "custom error message");
 *     assert.fail(1, 2, "custom error message", ">");
 *     assert.fail(1, 2, undefined, ">");
 *
 * @name fail
 * @param {unknown} actual
 * @param {unknown} expected
 * @param {string} message
 * @param {string} operator
 * @namespace Assert
 * @public
 */
assert.fail = function (actual, expected, message, operator) {
  if (arguments.length < 2) {
    // Comply with Node's fail([message]) interface

    message = actual;
    actual = undefined;
  }

  message = message || 'assert.fail()';
  throw new assertion_error__WEBPACK_IMPORTED_MODULE_3__.AssertionError(
    message,
    {
      actual: actual,
      expected: expected,
      operator: operator
    },
    assert.fail
  );
};

/**
 * ### .isOk(object, [message])
 *
 * Asserts that `object` is truthy.
 *
 *     assert.isOk('everything', 'everything is ok');
 *     assert.isOk(false, 'this will fail');
 *
 * @name isOk
 * @alias ok
 * @param {unknown} val object to test
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isOk = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isOk, true).is.ok;
};

/**
 * ### .isNotOk(object, [message])
 *
 * Asserts that `object` is falsy.
 *
 *     assert.isNotOk('everything', 'this will fail');
 *     assert.isNotOk(false, 'this will pass');
 *
 * @name isNotOk
 * @alias notOk
 * @param {unknown} val object to test
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotOk = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotOk, true).is.not.ok;
};

/**
 * ### .equal(actual, expected, [message])
 *
 * Asserts non-strict equality (`==`) of `actual` and `expected`.
 *
 *     assert.equal(3, '3', '== coerces values to strings');
 *
 * @name equal
 * @param {unknown} act
 * @param {unknown} exp
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.equal = function (act, exp, msg) {
  let test = new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.equal, true);

  test.assert(
    exp == (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag)(test, 'object'),
    'expected #{this} to equal #{exp}',
    'expected #{this} to not equal #{act}',
    exp,
    act,
    true
  );
};

/**
 * ### .notEqual(actual, expected, [message])
 *
 * Asserts non-strict inequality (`!=`) of `actual` and `expected`.
 *
 *     assert.notEqual(3, 4, 'these numbers are not equal');
 *
 * @name notEqual
 * @param {unknown} act
 * @param {unknown} exp
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notEqual = function (act, exp, msg) {
  let test = new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.notEqual, true);

  test.assert(
    exp != (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag)(test, 'object'),
    'expected #{this} to not equal #{exp}',
    'expected #{this} to equal #{act}',
    exp,
    act,
    true
  );
};

/**
 * ### .strictEqual(actual, expected, [message])
 *
 * Asserts strict equality (`===`) of `actual` and `expected`.
 *
 *     assert.strictEqual(true, true, 'these booleans are strictly equal');
 *
 * @name strictEqual
 * @param {unknown} act
 * @param {unknown} exp
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.strictEqual = function (act, exp, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.strictEqual, true).to.equal(exp);
};

/**
 * ### .notStrictEqual(actual, expected, [message])
 *
 * Asserts strict inequality (`!==`) of `actual` and `expected`.
 *
 *     assert.notStrictEqual(3, '3', 'no coercion for strict equality');
 *
 * @name notStrictEqual
 * @param {unknown} act
 * @param {unknown} exp
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notStrictEqual = function (act, exp, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.notStrictEqual, true).to.not.equal(exp);
};

/**
 * ### .deepEqual(actual, expected, [message])
 *
 * Asserts that `actual` is deeply equal to `expected`.
 *
 *     assert.deepEqual({ tea: 'green' }, { tea: 'green' });
 *
 * @name deepEqual
 * @param {unknown} act
 * @param {unknown} exp
 * @param {string} msg
 * @alias deepStrictEqual
 * @namespace Assert
 * @public
 */
assert.deepEqual = assert.deepStrictEqual = function (act, exp, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.deepEqual, true).to.eql(exp);
};

/**
 * ### .notDeepEqual(actual, expected, [message])
 *
 * Assert that `actual` is not deeply equal to `expected`.
 *
 *     assert.notDeepEqual({ tea: 'green' }, { tea: 'jasmine' });
 *
 * @name notDeepEqual
 * @param {unknown} act
 * @param {unknown} exp
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notDeepEqual = function (act, exp, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.notDeepEqual, true).to.not.eql(exp);
};

/**
 * ### .isAbove(valueToCheck, valueToBeAbove, [message])
 *
 * Asserts `valueToCheck` is strictly greater than (>) `valueToBeAbove`.
 *
 *     assert.isAbove(5, 2, '5 is strictly greater than 2');
 *
 * @name isAbove
 * @param {unknown} val
 * @param {unknown} abv
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isAbove = function (val, abv, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isAbove, true).to.be.above(abv);
};

/**
 * ### .isAtLeast(valueToCheck, valueToBeAtLeast, [message])
 *
 * Asserts `valueToCheck` is greater than or equal to (>=) `valueToBeAtLeast`.
 *
 *     assert.isAtLeast(5, 2, '5 is greater or equal to 2');
 *     assert.isAtLeast(3, 3, '3 is greater or equal to 3');
 *
 * @name isAtLeast
 * @param {unknown} val
 * @param {unknown} atlst
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isAtLeast = function (val, atlst, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isAtLeast, true).to.be.least(atlst);
};

/**
 * ### .isBelow(valueToCheck, valueToBeBelow, [message])
 *
 * Asserts `valueToCheck` is strictly less than (<) `valueToBeBelow`.
 *
 *     assert.isBelow(3, 6, '3 is strictly less than 6');
 *
 * @name isBelow
 * @param {unknown} val
 * @param {unknown} blw
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isBelow = function (val, blw, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isBelow, true).to.be.below(blw);
};

/**
 * ### .isAtMost(valueToCheck, valueToBeAtMost, [message])
 *
 * Asserts `valueToCheck` is less than or equal to (<=) `valueToBeAtMost`.
 *
 *     assert.isAtMost(3, 6, '3 is less than or equal to 6');
 *     assert.isAtMost(4, 4, '4 is less than or equal to 4');
 *
 * @name isAtMost
 * @param {unknown} val
 * @param {unknown} atmst
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isAtMost = function (val, atmst, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isAtMost, true).to.be.most(atmst);
};

/**
 * ### .isTrue(value, [message])
 *
 * Asserts that `value` is true.
 *
 *     var teaServed = true;
 *     assert.isTrue(teaServed, 'the tea has been served');
 *
 * @name isTrue
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isTrue = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isTrue, true).is['true'];
};

/**
 * ### .isNotTrue(value, [message])
 *
 * Asserts that `value` is not true.
 *
 *     var tea = 'tasty chai';
 *     assert.isNotTrue(tea, 'great, time for tea!');
 *
 * @name isNotTrue
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotTrue = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotTrue, true).to.not.equal(true);
};

/**
 * ### .isFalse(value, [message])
 *
 * Asserts that `value` is false.
 *
 *     var teaServed = false;
 *     assert.isFalse(teaServed, 'no tea yet? hmm...');
 *
 * @name isFalse
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isFalse = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isFalse, true).is['false'];
};

/**
 * ### .isNotFalse(value, [message])
 *
 * Asserts that `value` is not false.
 *
 *     var tea = 'tasty chai';
 *     assert.isNotFalse(tea, 'great, time for tea!');
 *
 * @name isNotFalse
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotFalse = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotFalse, true).to.not.equal(false);
};

/**
 * ### .isNull(value, [message])
 *
 * Asserts that `value` is null.
 *
 *     assert.isNull(err, 'there was no error');
 *
 * @name isNull
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNull = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNull, true).to.equal(null);
};

/**
 * ### .isNotNull(value, [message])
 *
 * Asserts that `value` is not null.
 *
 *     var tea = 'tasty chai';
 *     assert.isNotNull(tea, 'great, time for tea!');
 *
 * @name isNotNull
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotNull = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotNull, true).to.not.equal(null);
};

/**
 * ### .isNaN
 *
 * Asserts that value is NaN.
 *
 *     assert.isNaN(NaN, 'NaN is NaN');
 *
 * @name isNaN
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNaN = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNaN, true).to.be.NaN;
};

/**
 * ### .isNotNaN
 *
 * Asserts that value is not NaN.
 *
 *     assert.isNotNaN(4, '4 is not NaN');
 *
 * @name isNotNaN
 * @param {unknown} value
 * @param {string} message
 * @namespace Assert
 * @public
 */
assert.isNotNaN = function (value, message) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(value, message, assert.isNotNaN, true).not.to.be.NaN;
};

/**
 * ### .exists
 *
 * Asserts that the target is neither `null` nor `undefined`.
 *
 *     var foo = 'hi';
 *     assert.exists(foo, 'foo is neither `null` nor `undefined`');
 *
 * @name exists
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.exists = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.exists, true).to.exist;
};

/**
 * ### .notExists
 *
 * Asserts that the target is either `null` or `undefined`.
 *
 *     var bar = null
 *     , baz;
 *
 *     assert.notExists(bar);
 *     assert.notExists(baz, 'baz is either null or undefined');
 *
 * @name notExists
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notExists = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.notExists, true).to.not.exist;
};

/**
 * ### .isUndefined(value, [message])
 *
 * Asserts that `value` is `undefined`.
 *
 *     var tea;
 *     assert.isUndefined(tea, 'no tea defined');
 *
 * @name isUndefined
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isUndefined = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isUndefined, true).to.equal(undefined);
};

/**
 * ### .isDefined(value, [message])
 *
 * Asserts that `value` is not `undefined`.
 *
 *     var tea = 'cup of chai';
 *     assert.isDefined(tea, 'tea has been defined');
 *
 * @name isDefined
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isDefined = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isDefined, true).to.not.equal(undefined);
};

/**
 * ### .isCallable(value, [message])
 *
 * Asserts that `value` is a callable function.
 *
 *     function serveTea() { return 'cup of tea'; };
 *     assert.isCallable(serveTea, 'great, we can have tea now');
 *
 * @name isCallable
 * @param {unknown} value
 * @param {string} message
 * @namespace Assert
 * @public
 */
assert.isCallable = function (value, message) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(value, message, assert.isCallable, true).is.callable;
};

/**
 * ### .isNotCallable(value, [message])
 *
 * Asserts that `value` is _not_ a callable function.
 *
 *     var serveTea = [ 'heat', 'pour', 'sip' ];
 *     assert.isNotCallable(serveTea, 'great, we have listed the steps');
 *
 * @name isNotCallable
 * @param {unknown} value
 * @param {string} message
 * @namespace Assert
 * @public
 */
assert.isNotCallable = function (value, message) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(value, message, assert.isNotCallable, true).is.not.callable;
};

/**
 * ### .isObject(value, [message])
 *
 * Asserts that `value` is an object of type 'Object' (as revealed by `Object.prototype.toString`).
 * _The assertion does not match subclassed objects._
 *
 *     var selection = { name: 'Chai', serve: 'with spices' };
 *     assert.isObject(selection, 'tea selection is an object');
 *
 * @name isObject
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isObject = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isObject, true).to.be.a('object');
};

/**
 * ### .isNotObject(value, [message])
 *
 * Asserts that `value` is _not_ an object of type 'Object' (as revealed by `Object.prototype.toString`).
 *
 *     var selection = 'chai'
 *     assert.isNotObject(selection, 'tea selection is not an object');
 *     assert.isNotObject(null, 'null is not an object');
 *
 * @name isNotObject
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotObject = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotObject, true).to.not.be.a('object');
};

/**
 * ### .isArray(value, [message])
 *
 * Asserts that `value` is an array.
 *
 *     var menu = [ 'green', 'chai', 'oolong' ];
 *     assert.isArray(menu, 'what kind of tea do we want?');
 *
 * @name isArray
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isArray = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isArray, true).to.be.an('array');
};

/**
 * ### .isNotArray(value, [message])
 *
 * Asserts that `value` is _not_ an array.
 *
 *     var menu = 'green|chai|oolong';
 *     assert.isNotArray(menu, 'what kind of tea do we want?');
 *
 * @name isNotArray
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotArray = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotArray, true).to.not.be.an('array');
};

/**
 * ### .isString(value, [message])
 *
 * Asserts that `value` is a string.
 *
 *     var teaOrder = 'chai';
 *     assert.isString(teaOrder, 'order placed');
 *
 * @name isString
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isString = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isString, true).to.be.a('string');
};

/**
 * ### .isNotString(value, [message])
 *
 * Asserts that `value` is _not_ a string.
 *
 *     var teaOrder = 4;
 *     assert.isNotString(teaOrder, 'order placed');
 *
 * @name isNotString
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotString = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotString, true).to.not.be.a('string');
};

/**
 * ### .isNumber(value, [message])
 *
 * Asserts that `value` is a number.
 *
 *     var cups = 2;
 *     assert.isNumber(cups, 'how many cups');
 *
 * @name isNumber
 * @param {number} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNumber = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNumber, true).to.be.a('number');
};

/**
 * ### .isNotNumber(value, [message])
 *
 * Asserts that `value` is _not_ a number.
 *
 *     var cups = '2 cups please';
 *     assert.isNotNumber(cups, 'how many cups');
 *
 * @name isNotNumber
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotNumber = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotNumber, true).to.not.be.a('number');
};

/**
 * ### .isNumeric(value, [message])
 *
 * Asserts that `value` is a number or BigInt.
 *
 *     var cups = 2;
 *     assert.isNumeric(cups, 'how many cups');
 *
 *     var cups = 10n;
 *     assert.isNumeric(cups, 'how many cups');
 *
 * @name isNumeric
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNumeric = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNumeric, true).is.numeric;
};

/**
 * ### .isNotNumeric(value, [message])
 *
 * Asserts that `value` is _not_ a number or BigInt.
 *
 *     var cups = '2 cups please';
 *     assert.isNotNumeric(cups, 'how many cups');
 *
 * @name isNotNumeric
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotNumeric = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotNumeric, true).is.not.numeric;
};

/**
 * ### .isFinite(value, [message])
 *
 * Asserts that `value` is a finite number. Unlike `.isNumber`, this will fail for `NaN` and `Infinity`.
 *
 *     var cups = 2;
 *     assert.isFinite(cups, 'how many cups');
 *     assert.isFinite(NaN); // throws
 *
 * @name isFinite
 * @param {number} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isFinite = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isFinite, true).to.be.finite;
};

/**
 * ### .isBoolean(value, [message])
 *
 * Asserts that `value` is a boolean.
 *
 *     var teaReady = true
 *     , teaServed = false;
 *
 *     assert.isBoolean(teaReady, 'is the tea ready');
 *     assert.isBoolean(teaServed, 'has tea been served');
 *
 * @name isBoolean
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isBoolean = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isBoolean, true).to.be.a('boolean');
};

/**
 * ### .isNotBoolean(value, [message])
 *
 * Asserts that `value` is _not_ a boolean.
 *
 *     var teaReady = 'yep'
 *     , teaServed = 'nope';
 *
 *     assert.isNotBoolean(teaReady, 'is the tea ready');
 *     assert.isNotBoolean(teaServed, 'has tea been served');
 *
 * @name isNotBoolean
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.isNotBoolean = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotBoolean, true).to.not.be.a('boolean');
};

/**
 * ### .typeOf(value, name, [message])
 *
 * Asserts that `value`'s type is `name`, as determined by
 * `Object.prototype.toString`.
 *
 *     assert.typeOf({ tea: 'chai' }, 'object', 'we have an object');
 *     assert.typeOf(['chai', 'jasmine'], 'array', 'we have an array');
 *     assert.typeOf('tea', 'string', 'we have a string');
 *     assert.typeOf(/tea/, 'regexp', 'we have a regular expression');
 *     assert.typeOf(null, 'null', 'we have a null');
 *     assert.typeOf(undefined, 'undefined', 'we have an undefined');
 *
 * @name typeOf
 * @param {unknown} val
 * @param {string} type
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.typeOf = function (val, type, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.typeOf, true).to.be.a(type);
};

/**
 * ### .notTypeOf(value, name, [message])
 *
 * Asserts that `value`'s type is _not_ `name`, as determined by
 * `Object.prototype.toString`.
 *
 *     assert.notTypeOf('tea', 'number', 'strings are not numbers');
 *
 * @name notTypeOf
 * @param {unknown} value
 * @param {string} type
 * @param {string} message
 * @namespace Assert
 * @public
 */
assert.notTypeOf = function (value, type, message) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(value, message, assert.notTypeOf, true).to.not.be.a(type);
};

/**
 * ### .instanceOf(object, constructor, [message])
 *
 * Asserts that `value` is an instance of `constructor`.
 *
 *     var Tea = function (name) { this.name = name; }
 *     , chai = new Tea('chai');
 *
 *     assert.instanceOf(chai, Tea, 'chai is an instance of tea');
 *
 * @name instanceOf
 * @param {object} val
 * @param {object} type
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.instanceOf = function (val, type, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.instanceOf, true).to.be.instanceOf(type);
};

/**
 * ### .notInstanceOf(object, constructor, [message])
 *
 * Asserts `value` is not an instance of `constructor`.
 *
 *     var Tea = function (name) { this.name = name; }
 *     , chai = new String('chai');
 *
 *     assert.notInstanceOf(chai, Tea, 'chai is not an instance of tea');
 *
 * @name notInstanceOf
 * @param {object} val
 * @param {object} type
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notInstanceOf = function (val, type, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.notInstanceOf, true).to.not.be.instanceOf(
    type
  );
};

/**
 * ### .include(haystack, needle, [message])
 *
 * Asserts that `haystack` includes `needle`. Can be used to assert the
 * inclusion of a value in an array, a substring in a string, or a subset of
 * properties in an object.
 *
 *     assert.include([1,2,3], 2, 'array contains value');
 *     assert.include('foobar', 'foo', 'string contains substring');
 *     assert.include({ foo: 'bar', hello: 'universe' }, { foo: 'bar' }, 'object contains property');
 *
 * Strict equality (===) is used. When asserting the inclusion of a value in
 * an array, the array is searched for an element that's strictly equal to the
 * given value. When asserting a subset of properties in an object, the object
 * is searched for the given property keys, checking that each one is present
 * and strictly equal to the given property value. For instance:
 *
 *     var obj1 = {a: 1}
 *     , obj2 = {b: 2};
 *     assert.include([obj1, obj2], obj1);
 *     assert.include({foo: obj1, bar: obj2}, {foo: obj1});
 *     assert.include({foo: obj1, bar: obj2}, {foo: obj1, bar: obj2});
 *
 * @name include
 * @param {Array | string} exp
 * @param {unknown} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.include = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.include, true).include(inc);
};

/**
 * ### .notInclude(haystack, needle, [message])
 *
 * Asserts that `haystack` does not include `needle`. Can be used to assert
 * the absence of a value in an array, a substring in a string, or a subset of
 * properties in an object.
 *
 *     assert.notInclude([1,2,3], 4, "array doesn't contain value");
 *     assert.notInclude('foobar', 'baz', "string doesn't contain substring");
 *     assert.notInclude({ foo: 'bar', hello: 'universe' }, { foo: 'baz' }, 'object doesn't contain property');
 *
 * Strict equality (===) is used. When asserting the absence of a value in an
 * array, the array is searched to confirm the absence of an element that's
 * strictly equal to the given value. When asserting a subset of properties in
 * an object, the object is searched to confirm that at least one of the given
 * property keys is either not present or not strictly equal to the given
 * property value. For instance:
 *
 *     var obj1 = {a: 1}
 *     , obj2 = {b: 2};
 *     assert.notInclude([obj1, obj2], {a: 1});
 *     assert.notInclude({foo: obj1, bar: obj2}, {foo: {a: 1}});
 *     assert.notInclude({foo: obj1, bar: obj2}, {foo: obj1, bar: {b: 2}});
 *
 * @name notInclude
 * @param {Array | string} exp
 * @param {unknown} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.notInclude, true).not.include(inc);
};

/**
 * ### .deepInclude(haystack, needle, [message])
 *
 * Asserts that `haystack` includes `needle`. Can be used to assert the
 * inclusion of a value in an array or a subset of properties in an object.
 * Deep equality is used.
 *
 *     var obj1 = {a: 1}
 *     , obj2 = {b: 2};
 *     assert.deepInclude([obj1, obj2], {a: 1});
 *     assert.deepInclude({foo: obj1, bar: obj2}, {foo: {a: 1}});
 *     assert.deepInclude({foo: obj1, bar: obj2}, {foo: {a: 1}, bar: {b: 2}});
 *
 * @name deepInclude
 * @param {Array | string} exp
 * @param {unknown} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.deepInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.deepInclude, true).deep.include(inc);
};

/**
 * ### .notDeepInclude(haystack, needle, [message])
 *
 * Asserts that `haystack` does not include `needle`. Can be used to assert
 * the absence of a value in an array or a subset of properties in an object.
 * Deep equality is used.
 *
 *     var obj1 = {a: 1}
 *     , obj2 = {b: 2};
 *     assert.notDeepInclude([obj1, obj2], {a: 9});
 *     assert.notDeepInclude({foo: obj1, bar: obj2}, {foo: {a: 9}});
 *     assert.notDeepInclude({foo: obj1, bar: obj2}, {foo: {a: 1}, bar: {b: 9}});
 *
 * @name notDeepInclude
 * @param {Array | string} exp
 * @param {unknown} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notDeepInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.notDeepInclude, true).not.deep.include(inc);
};

/**
 * ### .nestedInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' includes 'needle'.
 * Can be used to assert the inclusion of a subset of properties in an
 * object.
 * Enables the use of dot- and bracket-notation for referencing nested
 * properties.
 * '[]' and '.' in property names can be escaped using double backslashes.
 *
 *     assert.nestedInclude({'.a': {'b': 'x'}}, {'\\.a.[b]': 'x'});
 *     assert.nestedInclude({'a': {'[b]': 'x'}}, {'a.\\[b\\]': 'x'});
 *
 * @name nestedInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.nestedInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.nestedInclude, true).nested.include(inc);
};

/**
 * ### .notNestedInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' does not include 'needle'.
 * Can be used to assert the absence of a subset of properties in an
 * object.
 * Enables the use of dot- and bracket-notation for referencing nested
 * properties.
 * '[]' and '.' in property names can be escaped using double backslashes.
 *
 *     assert.notNestedInclude({'.a': {'b': 'x'}}, {'\\.a.b': 'y'});
 *     assert.notNestedInclude({'a': {'[b]': 'x'}}, {'a.\\[b\\]': 'y'});
 *
 * @name notNestedInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notNestedInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.notNestedInclude, true).not.nested.include(
    inc
  );
};

/**
 * ### .deepNestedInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' includes 'needle'.
 * Can be used to assert the inclusion of a subset of properties in an
 * object while checking for deep equality.
 * Enables the use of dot- and bracket-notation for referencing nested
 * properties.
 * '[]' and '.' in property names can be escaped using double backslashes.
 *
 *     assert.deepNestedInclude({a: {b: [{x: 1}]}}, {'a.b[0]': {x: 1}});
 *     assert.deepNestedInclude({'.a': {'[b]': {x: 1}}}, {'\\.a.\\[b\\]': {x: 1}});
 *
 * @name deepNestedInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.deepNestedInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.deepNestedInclude, true).deep.nested.include(
    inc
  );
};

/**
 * ### .notDeepNestedInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' does not include 'needle'.
 * Can be used to assert the absence of a subset of properties in an
 * object while checking for deep equality.
 * Enables the use of dot- and bracket-notation for referencing nested
 * properties.
 * '[]' and '.' in property names can be escaped using double backslashes.
 *
 *     assert.notDeepNestedInclude({a: {b: [{x: 1}]}}, {'a.b[0]': {y: 1}})
 *     assert.notDeepNestedInclude({'.a': {'[b]': {x: 1}}}, {'\\.a.\\[b\\]': {y: 2}});
 *
 * @name notDeepNestedInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notDeepNestedInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    exp,
    msg,
    assert.notDeepNestedInclude,
    true
  ).not.deep.nested.include(inc);
};

/**
 * ### .ownInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' includes 'needle'.
 * Can be used to assert the inclusion of a subset of properties in an
 * object while ignoring inherited properties.
 *
 *     assert.ownInclude({ a: 1 }, { a: 1 });
 *
 * @name ownInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.ownInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.ownInclude, true).own.include(inc);
};

/**
 * ### .notOwnInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' does not include 'needle'.
 * Can be used to assert the absence of a subset of properties in an
 * object while ignoring inherited properties.
 *
 *     Object.prototype.b = 2;
 *     assert.notOwnInclude({ a: 1 }, { b: 2 });
 *
 * @name notOwnInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notOwnInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.notOwnInclude, true).not.own.include(inc);
};

/**
 * ### .deepOwnInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' includes 'needle'.
 * Can be used to assert the inclusion of a subset of properties in an
 * object while ignoring inherited properties and checking for deep equality.
 *
 *     assert.deepOwnInclude({a: {b: 2}}, {a: {b: 2}});
 *
 * @name deepOwnInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.deepOwnInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.deepOwnInclude, true).deep.own.include(inc);
};

/**
 * ### .notDeepOwnInclude(haystack, needle, [message])
 *
 * Asserts that 'haystack' includes 'needle'.
 * Can be used to assert the absence of a subset of properties in an
 * object while ignoring inherited properties and checking for deep equality.
 *
 *     assert.notDeepOwnInclude({a: {b: 2}}, {a: {c: 3}});
 *
 * @name notDeepOwnInclude
 * @param {object} exp
 * @param {object} inc
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notDeepOwnInclude = function (exp, inc, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.notDeepOwnInclude, true).not.deep.own.include(
    inc
  );
};

/**
 * ### .match(value, regexp, [message])
 *
 * Asserts that `value` matches the regular expression `regexp`.
 *
 *     assert.match('foobar', /^foo/, 'regexp matches');
 *
 * @name match
 * @param {unknown} exp
 * @param {RegExp} re
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.match = function (exp, re, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.match, true).to.match(re);
};

/**
 * ### .notMatch(value, regexp, [message])
 *
 * Asserts that `value` does not match the regular expression `regexp`.
 *
 *     assert.notMatch('foobar', /^foo/, 'regexp does not match');
 *
 * @name notMatch
 * @param {unknown} exp
 * @param {RegExp} re
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notMatch = function (exp, re, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.notMatch, true).to.not.match(re);
};

/**
 * ### .property(object, property, [message])
 *
 * Asserts that `object` has a direct or inherited property named by
 * `property`.
 *
 *     assert.property({ tea: { green: 'matcha' }}, 'tea');
 *     assert.property({ tea: { green: 'matcha' }}, 'toString');
 *
 * @name property
 * @param {object} obj
 * @param {string} prop
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.property = function (obj, prop, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.property, true).to.have.property(prop);
};

/**
 * ### .notProperty(object, property, [message])
 *
 * Asserts that `object` does _not_ have a direct or inherited property named
 * by `property`.
 *
 *     assert.notProperty({ tea: { green: 'matcha' }}, 'coffee');
 *
 * @name notProperty
 * @param {object} obj
 * @param {string} prop
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notProperty = function (obj, prop, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.notProperty, true).to.not.have.property(prop);
};

/**
 * ### .propertyVal(object, property, value, [message])
 *
 * Asserts that `object` has a direct or inherited property named by
 * `property` with a value given by `value`. Uses a strict equality check
 * (===).
 *
 *     assert.propertyVal({ tea: 'is good' }, 'tea', 'is good');
 *
 * @name propertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.propertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.propertyVal, true).to.have.property(prop, val);
};

/**
 * ### .notPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` does _not_ have a direct or inherited property named
 * by `property` with value given by `value`. Uses a strict equality check
 * (===).
 *
 *     assert.notPropertyVal({ tea: 'is good' }, 'tea', 'is bad');
 *     assert.notPropertyVal({ tea: 'is good' }, 'coffee', 'is good');
 *
 * @name notPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notPropertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.notPropertyVal, true).to.not.have.property(
    prop,
    val
  );
};

/**
 * ### .deepPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` has a direct or inherited property named by
 * `property` with a value given by `value`. Uses a deep equality check.
 *
 *     assert.deepPropertyVal({ tea: { green: 'matcha' } }, 'tea', { green: 'matcha' });
 *
 * @name deepPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.deepPropertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.deepPropertyVal, true).to.have.deep.property(
    prop,
    val
  );
};

/**
 * ### .notDeepPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` does _not_ have a direct or inherited property named
 * by `property` with value given by `value`. Uses a deep equality check.
 *
 *     assert.notDeepPropertyVal({ tea: { green: 'matcha' } }, 'tea', { black: 'matcha' });
 *     assert.notDeepPropertyVal({ tea: { green: 'matcha' } }, 'tea', { green: 'oolong' });
 *     assert.notDeepPropertyVal({ tea: { green: 'matcha' } }, 'coffee', { green: 'matcha' });
 *
 * @name notDeepPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notDeepPropertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.notDeepPropertyVal,
    true
  ).to.not.have.deep.property(prop, val);
};

/**
 * ### .ownProperty(object, property, [message])
 *
 * Asserts that `object` has a direct property named by `property`. Inherited
 * properties aren't checked.
 *
 *     assert.ownProperty({ tea: { green: 'matcha' }}, 'tea');
 *
 * @name ownProperty
 * @param {object} obj
 * @param {string} prop
 * @param {string} msg
 * @public
 */
assert.ownProperty = function (obj, prop, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.ownProperty, true).to.have.own.property(prop);
};

/**
 * ### .notOwnProperty(object, property, [message])
 *
 * Asserts that `object` does _not_ have a direct property named by
 * `property`. Inherited properties aren't checked.
 *
 *     assert.notOwnProperty({ tea: { green: 'matcha' }}, 'coffee');
 *     assert.notOwnProperty({}, 'toString');
 *
 * @name notOwnProperty
 * @param {object} obj
 * @param {string} prop
 * @param {string} msg
 * @public
 */
assert.notOwnProperty = function (obj, prop, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.notOwnProperty, true).to.not.have.own.property(
    prop
  );
};

/**
 * ### .ownPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` has a direct property named by `property` and a value
 * equal to the provided `value`. Uses a strict equality check (===).
 * Inherited properties aren't checked.
 *
 *     assert.ownPropertyVal({ coffee: 'is good'}, 'coffee', 'is good');
 *
 * @name ownPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} value
 * @param {string} msg
 * @public
 */
assert.ownPropertyVal = function (obj, prop, value, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.ownPropertyVal, true).to.have.own.property(
    prop,
    value
  );
};

/**
 * ### .notOwnPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` does _not_ have a direct property named by `property`
 * with a value equal to the provided `value`. Uses a strict equality check
 * (===). Inherited properties aren't checked.
 *
 *     assert.notOwnPropertyVal({ tea: 'is better'}, 'tea', 'is worse');
 *     assert.notOwnPropertyVal({}, 'toString', Object.prototype.toString);
 *
 * @name notOwnPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} value
 * @param {string} msg
 * @public
 */
assert.notOwnPropertyVal = function (obj, prop, value, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.notOwnPropertyVal,
    true
  ).to.not.have.own.property(prop, value);
};

/**
 * ### .deepOwnPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` has a direct property named by `property` and a value
 * equal to the provided `value`. Uses a deep equality check. Inherited
 * properties aren't checked.
 *
 *     assert.deepOwnPropertyVal({ tea: { green: 'matcha' } }, 'tea', { green: 'matcha' });
 *
 * @name deepOwnPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} value
 * @param {string} msg
 * @public
 */
assert.deepOwnPropertyVal = function (obj, prop, value, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.deepOwnPropertyVal,
    true
  ).to.have.deep.own.property(prop, value);
};

/**
 * ### .notDeepOwnPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` does _not_ have a direct property named by `property`
 * with a value equal to the provided `value`. Uses a deep equality check.
 * Inherited properties aren't checked.
 *
 *     assert.notDeepOwnPropertyVal({ tea: { green: 'matcha' } }, 'tea', { black: 'matcha' });
 *     assert.notDeepOwnPropertyVal({ tea: { green: 'matcha' } }, 'tea', { green: 'oolong' });
 *     assert.notDeepOwnPropertyVal({ tea: { green: 'matcha' } }, 'coffee', { green: 'matcha' });
 *     assert.notDeepOwnPropertyVal({}, 'toString', Object.prototype.toString);
 *
 * @name notDeepOwnPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} value
 * @param {string} msg
 * @public
 */
assert.notDeepOwnPropertyVal = function (obj, prop, value, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.notDeepOwnPropertyVal,
    true
  ).to.not.have.deep.own.property(prop, value);
};

/**
 * ### .nestedProperty(object, property, [message])
 *
 * Asserts that `object` has a direct or inherited property named by
 * `property`, which can be a string using dot- and bracket-notation for
 * nested reference.
 *
 *     assert.nestedProperty({ tea: { green: 'matcha' }}, 'tea.green');
 *
 * @name nestedProperty
 * @param {object} obj
 * @param {string} prop
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.nestedProperty = function (obj, prop, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.nestedProperty, true).to.have.nested.property(
    prop
  );
};

/**
 * ### .notNestedProperty(object, property, [message])
 *
 * Asserts that `object` does _not_ have a property named by `property`, which
 * can be a string using dot- and bracket-notation for nested reference. The
 * property cannot exist on the object nor anywhere in its prototype chain.
 *
 *     assert.notNestedProperty({ tea: { green: 'matcha' }}, 'tea.oolong');
 *
 * @name notNestedProperty
 * @param {object} obj
 * @param {string} prop
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notNestedProperty = function (obj, prop, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.notNestedProperty,
    true
  ).to.not.have.nested.property(prop);
};

/**
 * ### .nestedPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` has a property named by `property` with value given
 * by `value`. `property` can use dot- and bracket-notation for nested
 * reference. Uses a strict equality check (===).
 *
 *     assert.nestedPropertyVal({ tea: { green: 'matcha' }}, 'tea.green', 'matcha');
 *
 * @name nestedPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.nestedPropertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.nestedPropertyVal,
    true
  ).to.have.nested.property(prop, val);
};

/**
 * ### .notNestedPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` does _not_ have a property named by `property` with
 * value given by `value`. `property` can use dot- and bracket-notation for
 * nested reference. Uses a strict equality check (===).
 *
 *     assert.notNestedPropertyVal({ tea: { green: 'matcha' }}, 'tea.green', 'konacha');
 *     assert.notNestedPropertyVal({ tea: { green: 'matcha' }}, 'coffee.green', 'matcha');
 *
 * @name notNestedPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notNestedPropertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.notNestedPropertyVal,
    true
  ).to.not.have.nested.property(prop, val);
};

/**
 * ### .deepNestedPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` has a property named by `property` with a value given
 * by `value`. `property` can use dot- and bracket-notation for nested
 * reference. Uses a deep equality check.
 *
 *     assert.deepNestedPropertyVal({ tea: { green: { matcha: 'yum' } } }, 'tea.green', { matcha: 'yum' });
 *
 * @name deepNestedPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.deepNestedPropertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.deepNestedPropertyVal,
    true
  ).to.have.deep.nested.property(prop, val);
};

/**
 * ### .notDeepNestedPropertyVal(object, property, value, [message])
 *
 * Asserts that `object` does _not_ have a property named by `property` with
 * value given by `value`. `property` can use dot- and bracket-notation for
 * nested reference. Uses a deep equality check.
 *
 *     assert.notDeepNestedPropertyVal({ tea: { green: { matcha: 'yum' } } }, 'tea.green', { oolong: 'yum' });
 *     assert.notDeepNestedPropertyVal({ tea: { green: { matcha: 'yum' } } }, 'tea.green', { matcha: 'yuck' });
 *     assert.notDeepNestedPropertyVal({ tea: { green: { matcha: 'yum' } } }, 'tea.black', { matcha: 'yum' });
 *
 * @name notDeepNestedPropertyVal
 * @param {object} obj
 * @param {string} prop
 * @param {unknown} val
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notDeepNestedPropertyVal = function (obj, prop, val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.notDeepNestedPropertyVal,
    true
  ).to.not.have.deep.nested.property(prop, val);
};

/**
 * ### .lengthOf(object, length, [message])
 *
 * Asserts that `object` has a `length` or `size` with the expected value.
 *
 *     assert.lengthOf([1,2,3], 3, 'array has length of 3');
 *     assert.lengthOf('foobar', 6, 'string has length of 6');
 *     assert.lengthOf(new Set([1,2,3]), 3, 'set has size of 3');
 *     assert.lengthOf(new Map([['a',1],['b',2],['c',3]]), 3, 'map has size of 3');
 *
 * @name lengthOf
 * @param {unknown} exp
 * @param {number} len
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.lengthOf = function (exp, len, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(exp, msg, assert.lengthOf, true).to.have.lengthOf(len);
};

/**
 * ### .hasAnyKeys(object, [keys], [message])
 *
 * Asserts that `object` has at least one of the `keys` provided.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.hasAnyKeys({foo: 1, bar: 2, baz: 3}, ['foo', 'iDontExist', 'baz']);
 *     assert.hasAnyKeys({foo: 1, bar: 2, baz: 3}, {foo: 30, iDontExist: 99, baz: 1337});
 *     assert.hasAnyKeys(new Map([[{foo: 1}, 'bar'], ['key', 'value']]), [{foo: 1}, 'key']);
 *     assert.hasAnyKeys(new Set([{foo: 'bar'}, 'anotherKey']), [{foo: 'bar'}, 'anotherKey']);
 *
 * @name hasAnyKeys
 * @param {unknown} obj
 * @param {Array | object} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.hasAnyKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.hasAnyKeys, true).to.have.any.keys(keys);
};

/**
 * ### .hasAllKeys(object, [keys], [message])
 *
 * Asserts that `object` has all and only all of the `keys` provided.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.hasAllKeys({foo: 1, bar: 2, baz: 3}, ['foo', 'bar', 'baz']);
 *     assert.hasAllKeys({foo: 1, bar: 2, baz: 3}, {foo: 30, bar: 99, baz: 1337]);
 *     assert.hasAllKeys(new Map([[{foo: 1}, 'bar'], ['key', 'value']]), [{foo: 1}, 'key']);
 *     assert.hasAllKeys(new Set([{foo: 'bar'}, 'anotherKey']), [{foo: 'bar'}, 'anotherKey']);
 *
 * @name hasAllKeys
 * @param {unknown} obj
 * @param {string[]} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.hasAllKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.hasAllKeys, true).to.have.all.keys(keys);
};

/**
 * ### .containsAllKeys(object, [keys], [message])
 *
 * Asserts that `object` has all of the `keys` provided but may have more keys not listed.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.containsAllKeys({foo: 1, bar: 2, baz: 3}, ['foo', 'baz']);
 *     assert.containsAllKeys({foo: 1, bar: 2, baz: 3}, ['foo', 'bar', 'baz']);
 *     assert.containsAllKeys({foo: 1, bar: 2, baz: 3}, {foo: 30, baz: 1337});
 *     assert.containsAllKeys({foo: 1, bar: 2, baz: 3}, {foo: 30, bar: 99, baz: 1337});
 *     assert.containsAllKeys(new Map([[{foo: 1}, 'bar'], ['key', 'value']]), [{foo: 1}]);
 *     assert.containsAllKeys(new Map([[{foo: 1}, 'bar'], ['key', 'value']]), [{foo: 1}, 'key']);
 *     assert.containsAllKeys(new Set([{foo: 'bar'}, 'anotherKey']), [{foo: 'bar'}]);
 *     assert.containsAllKeys(new Set([{foo: 'bar'}, 'anotherKey']), [{foo: 'bar'}, 'anotherKey']);
 *
 * @name containsAllKeys
 * @param {unknown} obj
 * @param {string[]} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.containsAllKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.containsAllKeys, true).to.contain.all.keys(
    keys
  );
};

/**
 * ### .doesNotHaveAnyKeys(object, [keys], [message])
 *
 * Asserts that `object` has none of the `keys` provided.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.doesNotHaveAnyKeys({foo: 1, bar: 2, baz: 3}, ['one', 'two', 'example']);
 *     assert.doesNotHaveAnyKeys({foo: 1, bar: 2, baz: 3}, {one: 1, two: 2, example: 'foo'});
 *     assert.doesNotHaveAnyKeys(new Map([[{foo: 1}, 'bar'], ['key', 'value']]), [{one: 'two'}, 'example']);
 *     assert.doesNotHaveAnyKeys(new Set([{foo: 'bar'}, 'anotherKey']), [{one: 'two'}, 'example']);
 *
 * @name doesNotHaveAnyKeys
 * @param {unknown} obj
 * @param {string[]} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.doesNotHaveAnyKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.doesNotHaveAnyKeys, true).to.not.have.any.keys(
    keys
  );
};

/**
 * ### .doesNotHaveAllKeys(object, [keys], [message])
 *
 * Asserts that `object` does not have at least one of the `keys` provided.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.doesNotHaveAllKeys({foo: 1, bar: 2, baz: 3}, ['one', 'two', 'example']);
 *     assert.doesNotHaveAllKeys({foo: 1, bar: 2, baz: 3}, {one: 1, two: 2, example: 'foo'});
 *     assert.doesNotHaveAllKeys(new Map([[{foo: 1}, 'bar'], ['key', 'value']]), [{one: 'two'}, 'example']);
 *     assert.doesNotHaveAllKeys(new Set([{foo: 'bar'}, 'anotherKey']), [{one: 'two'}, 'example']);
 *
 * @name doesNotHaveAllKeys
 * @param {unknown} obj
 * @param {string[]} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.doesNotHaveAllKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.doesNotHaveAllKeys, true).to.not.have.all.keys(
    keys
  );
};

/**
 * ### .hasAnyDeepKeys(object, [keys], [message])
 *
 * Asserts that `object` has at least one of the `keys` provided.
 * Since Sets and Maps can have objects as keys you can use this assertion to perform
 * a deep comparison.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.hasAnyDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [1, 2]]), {one: 'one'});
 *     assert.hasAnyDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [1, 2]]), [{one: 'one'}, {two: 'two'}]);
 *     assert.hasAnyDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [{two: 'two'}, 'valueTwo']]), [{one: 'one'}, {two: 'two'}]);
 *     assert.hasAnyDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), {one: 'one'});
 *     assert.hasAnyDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), [{one: 'one'}, {three: 'three'}]);
 *     assert.hasAnyDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), [{one: 'one'}, {two: 'two'}]);
 *
 * @name hasAnyDeepKeys
 * @param {unknown} obj
 * @param {Array | object} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.hasAnyDeepKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.hasAnyDeepKeys, true).to.have.any.deep.keys(
    keys
  );
};

/**
 * ### .hasAllDeepKeys(object, [keys], [message])
 *
 * Asserts that `object` has all and only all of the `keys` provided.
 * Since Sets and Maps can have objects as keys you can use this assertion to perform
 * a deep comparison.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.hasAllDeepKeys(new Map([[{one: 'one'}, 'valueOne']]), {one: 'one'});
 *     assert.hasAllDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [{two: 'two'}, 'valueTwo']]), [{one: 'one'}, {two: 'two'}]);
 *     assert.hasAllDeepKeys(new Set([{one: 'one'}]), {one: 'one'});
 *     assert.hasAllDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), [{one: 'one'}, {two: 'two'}]);
 *
 * @name hasAllDeepKeys
 * @param {unknown} obj
 * @param {Array | object} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.hasAllDeepKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.hasAllDeepKeys, true).to.have.all.deep.keys(
    keys
  );
};

/**
 * ### .containsAllDeepKeys(object, [keys], [message])
 *
 * Asserts that `object` contains all of the `keys` provided.
 * Since Sets and Maps can have objects as keys you can use this assertion to perform
 * a deep comparison.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.containsAllDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [1, 2]]), {one: 'one'});
 *     assert.containsAllDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [{two: 'two'}, 'valueTwo']]), [{one: 'one'}, {two: 'two'}]);
 *     assert.containsAllDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), {one: 'one'});
 *     assert.containsAllDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), [{one: 'one'}, {two: 'two'}]);
 *
 * @name containsAllDeepKeys
 * @param {unknown} obj
 * @param {Array | object} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.containsAllDeepKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.containsAllDeepKeys,
    true
  ).to.contain.all.deep.keys(keys);
};

/**
 * ### .doesNotHaveAnyDeepKeys(object, [keys], [message])
 *
 * Asserts that `object` has none of the `keys` provided.
 * Since Sets and Maps can have objects as keys you can use this assertion to perform
 * a deep comparison.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.doesNotHaveAnyDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [1, 2]]), {thisDoesNot: 'exist'});
 *     assert.doesNotHaveAnyDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [{two: 'two'}, 'valueTwo']]), [{twenty: 'twenty'}, {fifty: 'fifty'}]);
 *     assert.doesNotHaveAnyDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), {twenty: 'twenty'});
 *     assert.doesNotHaveAnyDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), [{twenty: 'twenty'}, {fifty: 'fifty'}]);
 *
 * @name doesNotHaveAnyDeepKeys
 * @param {unknown} obj
 * @param {Array | object} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.doesNotHaveAnyDeepKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.doesNotHaveAnyDeepKeys,
    true
  ).to.not.have.any.deep.keys(keys);
};

/**
 * ### .doesNotHaveAllDeepKeys(object, [keys], [message])
 *
 * Asserts that `object` does not have at least one of the `keys` provided.
 * Since Sets and Maps can have objects as keys you can use this assertion to perform
 * a deep comparison.
 * You can also provide a single object instead of a `keys` array and its keys
 * will be used as the expected set of keys.
 *
 *     assert.doesNotHaveAllDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [1, 2]]), {thisDoesNot: 'exist'});
 *     assert.doesNotHaveAllDeepKeys(new Map([[{one: 'one'}, 'valueOne'], [{two: 'two'}, 'valueTwo']]), [{twenty: 'twenty'}, {one: 'one'}]);
 *     assert.doesNotHaveAllDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), {twenty: 'twenty'});
 *     assert.doesNotHaveAllDeepKeys(new Set([{one: 'one'}, {two: 'two'}]), [{one: 'one'}, {fifty: 'fifty'}]);
 *
 * @name doesNotHaveAllDeepKeys
 * @param {unknown} obj
 * @param {Array | object} keys
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.doesNotHaveAllDeepKeys = function (obj, keys, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    obj,
    msg,
    assert.doesNotHaveAllDeepKeys,
    true
  ).to.not.have.all.deep.keys(keys);
};

/**
 * ### .throws(fn, [errorLike/string/regexp], [string/regexp], [message])
 *
 * If `errorLike` is an `Error` constructor, asserts that `fn` will throw an error that is an
 * instance of `errorLike`.
 * If `errorLike` is an `Error` instance, asserts that the error thrown is the same
 * instance as `errorLike`.
 * If `errMsgMatcher` is provided, it also asserts that the error thrown will have a
 * message matching `errMsgMatcher`.
 *
 *     assert.throws(fn, 'Error thrown must have this msg');
 *     assert.throws(fn, /Error thrown must have a msg that matches this/);
 *     assert.throws(fn, ReferenceError);
 *     assert.throws(fn, errorInstance);
 *     assert.throws(fn, ReferenceError, 'Error thrown must be a ReferenceError and have this msg');
 *     assert.throws(fn, errorInstance, 'Error thrown must be the same errorInstance and have this msg');
 *     assert.throws(fn, ReferenceError, /Error thrown must be a ReferenceError and match this/);
 *     assert.throws(fn, errorInstance, /Error thrown must be the same errorInstance and match this/);
 *
 * @name throws
 * @alias throw
 * @alias Throw
 * @param {Function} fn
 * @param {Error} errorLike
 * @param {RegExp | string} errMsgMatcher
 * @param {string} msg
 * @returns {unknown}
 * @see https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error#Error_types
 * @namespace Assert
 * @public
 */
assert.throws = function (fn, errorLike, errMsgMatcher, msg) {
  if ('string' === typeof errorLike || errorLike instanceof RegExp) {
    errMsgMatcher = errorLike;
    errorLike = null;
  }

  let assertErr = new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.throws, true).to.throw(
    errorLike,
    errMsgMatcher
  );
  return (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag)(assertErr, 'object');
};

/**
 * ### .doesNotThrow(fn, [errorLike/string/regexp], [string/regexp], [message])
 *
 * If `errorLike` is an `Error` constructor, asserts that `fn` will _not_ throw an error that is an
 * instance of `errorLike`.
 * If `errorLike` is an `Error` instance, asserts that the error thrown is _not_ the same
 * instance as `errorLike`.
 * If `errMsgMatcher` is provided, it also asserts that the error thrown will _not_ have a
 * message matching `errMsgMatcher`.
 *
 *     assert.doesNotThrow(fn, 'Any Error thrown must not have this message');
 *     assert.doesNotThrow(fn, /Any Error thrown must not match this/);
 *     assert.doesNotThrow(fn, Error);
 *     assert.doesNotThrow(fn, errorInstance);
 *     assert.doesNotThrow(fn, Error, 'Error must not have this message');
 *     assert.doesNotThrow(fn, errorInstance, 'Error must not have this message');
 *     assert.doesNotThrow(fn, Error, /Error must not match this/);
 *     assert.doesNotThrow(fn, errorInstance, /Error must not match this/);
 *
 * @name doesNotThrow
 * @param {Function} fn
 * @param {Error} errorLike
 * @param {RegExp | string} errMsgMatcher
 * @param {string} message
 * @see https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error#Error_types
 * @namespace Assert
 * @public
 */
assert.doesNotThrow = function (fn, errorLike, errMsgMatcher, message) {
  if ('string' === typeof errorLike || errorLike instanceof RegExp) {
    errMsgMatcher = errorLike;
    errorLike = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, message, assert.doesNotThrow, true).to.not.throw(
    errorLike,
    errMsgMatcher
  );
};

/**
 * ### .operator(val1, operator, val2, [message])
 *
 * Compares two values using `operator`.
 *
 *     assert.operator(1, '<', 2, 'everything is ok');
 *     assert.operator(1, '>', 2, 'this will fail');
 *
 * @name operator
 * @param {unknown} val
 * @param {string} operator
 * @param {unknown} val2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.operator = function (val, operator, val2, msg) {
  let ok;
  switch (operator) {
    case '==':
      ok = val == val2;
      break;
    case '===':
      ok = val === val2;
      break;
    case '>':
      ok = val > val2;
      break;
    case '>=':
      ok = val >= val2;
      break;
    case '<':
      ok = val < val2;
      break;
    case '<=':
      ok = val <= val2;
      break;
    case '!=':
      ok = val != val2;
      break;
    case '!==':
      ok = val !== val2;
      break;
    default:
      msg = msg ? msg + ': ' : msg;
      throw new assertion_error__WEBPACK_IMPORTED_MODULE_3__.AssertionError(
        msg + 'Invalid operator "' + operator + '"',
        undefined,
        assert.operator
      );
  }
  let test = new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(ok, msg, assert.operator, true);
  test.assert(
    true === (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.flag)(test, 'object'),
    'expected ' + (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect)(val) + ' to be ' + operator + ' ' + (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect)(val2),
    'expected ' + (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect)(val) + ' to not be ' + operator + ' ' + (0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect)(val2)
  );
};

/**
 * ### .closeTo(actual, expected, delta, [message])
 *
 * Asserts that the target is equal `expected`, to within a +/- `delta` range.
 *
 *     assert.closeTo(1.5, 1, 0.5, 'numbers are close');
 *
 * @name closeTo
 * @param {number} act
 * @param {number} exp
 * @param {number} delta
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.closeTo = function (act, exp, delta, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.closeTo, true).to.be.closeTo(exp, delta);
};

/**
 * ### .approximately(actual, expected, delta, [message])
 *
 * Asserts that the target is equal `expected`, to within a +/- `delta` range.
 *
 *     assert.approximately(1.5, 1, 0.5, 'numbers are close');
 *
 * @name approximately
 * @param {number} act
 * @param {number} exp
 * @param {number} delta
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.approximately = function (act, exp, delta, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(act, msg, assert.approximately, true).to.be.approximately(
    exp,
    delta
  );
};

/**
 * ### .sameMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` have the same members in any order. Uses a
 * strict equality check (===).
 *
 *     assert.sameMembers([ 1, 2, 3 ], [ 2, 1, 3 ], 'same members');
 *
 * @name sameMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.sameMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(set1, msg, assert.sameMembers, true).to.have.same.members(set2);
};

/**
 * ### .notSameMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` don't have the same members in any order.
 * Uses a strict equality check (===).
 *
 *     assert.notSameMembers([ 1, 2, 3 ], [ 5, 1, 3 ], 'not same members');
 *
 * @name notSameMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notSameMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    set1,
    msg,
    assert.notSameMembers,
    true
  ).to.not.have.same.members(set2);
};

/**
 * ### .sameDeepMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` have the same members in any order. Uses a
 * deep equality check.
 *
 *     assert.sameDeepMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [{ b: 2 }, { a: 1 }, { c: 3 }], 'same deep members');
 *
 * @name sameDeepMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.sameDeepMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    set1,
    msg,
    assert.sameDeepMembers,
    true
  ).to.have.same.deep.members(set2);
};

/**
 * ### .notSameDeepMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` don't have the same members in any order.
 * Uses a deep equality check.
 *
 *     assert.notSameDeepMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [{ b: 2 }, { a: 1 }, { f: 5 }], 'not same deep members');
 *
 * @name notSameDeepMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notSameDeepMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    set1,
    msg,
    assert.notSameDeepMembers,
    true
  ).to.not.have.same.deep.members(set2);
};

/**
 * ### .sameOrderedMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` have the same members in the same order.
 * Uses a strict equality check (===).
 *
 *     assert.sameOrderedMembers([ 1, 2, 3 ], [ 1, 2, 3 ], 'same ordered members');
 *
 * @name sameOrderedMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.sameOrderedMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    set1,
    msg,
    assert.sameOrderedMembers,
    true
  ).to.have.same.ordered.members(set2);
};

/**
 * ### .notSameOrderedMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` don't have the same members in the same
 * order. Uses a strict equality check (===).
 *
 *     assert.notSameOrderedMembers([ 1, 2, 3 ], [ 2, 1, 3 ], 'not same ordered members');
 *
 * @name notSameOrderedMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notSameOrderedMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    set1,
    msg,
    assert.notSameOrderedMembers,
    true
  ).to.not.have.same.ordered.members(set2);
};

/**
 * ### .sameDeepOrderedMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` have the same members in the same order.
 * Uses a deep equality check.
 *
 *     assert.sameDeepOrderedMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { a: 1 }, { b: 2 }, { c: 3 } ], 'same deep ordered members');
 *
 * @name sameDeepOrderedMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.sameDeepOrderedMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    set1,
    msg,
    assert.sameDeepOrderedMembers,
    true
  ).to.have.same.deep.ordered.members(set2);
};

/**
 * ### .notSameDeepOrderedMembers(set1, set2, [message])
 *
 * Asserts that `set1` and `set2` don't have the same members in the same
 * order. Uses a deep equality check.
 *
 *     assert.notSameDeepOrderedMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { a: 1 }, { b: 2 }, { z: 5 } ], 'not same deep ordered members');
 *     assert.notSameDeepOrderedMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { b: 2 }, { a: 1 }, { c: 3 } ], 'not same deep ordered members');
 *
 * @name notSameDeepOrderedMembers
 * @param {Array} set1
 * @param {Array} set2
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notSameDeepOrderedMembers = function (set1, set2, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    set1,
    msg,
    assert.notSameDeepOrderedMembers,
    true
  ).to.not.have.same.deep.ordered.members(set2);
};

/**
 * ### .includeMembers(superset, subset, [message])
 *
 * Asserts that `subset` is included in `superset` in any order. Uses a
 * strict equality check (===). Duplicates are ignored.
 *
 *     assert.includeMembers([ 1, 2, 3 ], [ 2, 1, 2 ], 'include members');
 *
 * @name includeMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.includeMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(superset, msg, assert.includeMembers, true).to.include.members(
    subset
  );
};

/**
 * ### .notIncludeMembers(superset, subset, [message])
 *
 * Asserts that `subset` isn't included in `superset` in any order. Uses a
 * strict equality check (===). Duplicates are ignored.
 *
 *     assert.notIncludeMembers([ 1, 2, 3 ], [ 5, 1 ], 'not include members');
 *
 * @name notIncludeMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notIncludeMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    superset,
    msg,
    assert.notIncludeMembers,
    true
  ).to.not.include.members(subset);
};

/**
 * ### .includeDeepMembers(superset, subset, [message])
 *
 * Asserts that `subset` is included in `superset` in any order. Uses a deep
 * equality check. Duplicates are ignored.
 *
 *     assert.includeDeepMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { b: 2 }, { a: 1 }, { b: 2 } ], 'include deep members');
 *
 * @name includeDeepMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.includeDeepMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    superset,
    msg,
    assert.includeDeepMembers,
    true
  ).to.include.deep.members(subset);
};

/**
 * ### .notIncludeDeepMembers(superset, subset, [message])
 *
 * Asserts that `subset` isn't included in `superset` in any order. Uses a
 * deep equality check. Duplicates are ignored.
 *
 *     assert.notIncludeDeepMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { b: 2 }, { f: 5 } ], 'not include deep members');
 *
 * @name notIncludeDeepMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notIncludeDeepMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    superset,
    msg,
    assert.notIncludeDeepMembers,
    true
  ).to.not.include.deep.members(subset);
};

/**
 * ### .includeOrderedMembers(superset, subset, [message])
 *
 * Asserts that `subset` is included in `superset` in the same order
 * beginning with the first element in `superset`. Uses a strict equality
 * check (===).
 *
 *     assert.includeOrderedMembers([ 1, 2, 3 ], [ 1, 2 ], 'include ordered members');
 *
 * @name includeOrderedMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.includeOrderedMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    superset,
    msg,
    assert.includeOrderedMembers,
    true
  ).to.include.ordered.members(subset);
};

/**
 * ### .notIncludeOrderedMembers(superset, subset, [message])
 *
 * Asserts that `subset` isn't included in `superset` in the same order
 * beginning with the first element in `superset`. Uses a strict equality
 * check (===).
 *
 *     assert.notIncludeOrderedMembers([ 1, 2, 3 ], [ 2, 1 ], 'not include ordered members');
 *     assert.notIncludeOrderedMembers([ 1, 2, 3 ], [ 2, 3 ], 'not include ordered members');
 *
 * @name notIncludeOrderedMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notIncludeOrderedMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    superset,
    msg,
    assert.notIncludeOrderedMembers,
    true
  ).to.not.include.ordered.members(subset);
};

/**
 * ### .includeDeepOrderedMembers(superset, subset, [message])
 *
 * Asserts that `subset` is included in `superset` in the same order
 * beginning with the first element in `superset`. Uses a deep equality
 * check.
 *
 *     assert.includeDeepOrderedMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { a: 1 }, { b: 2 } ], 'include deep ordered members');
 *
 * @name includeDeepOrderedMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.includeDeepOrderedMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    superset,
    msg,
    assert.includeDeepOrderedMembers,
    true
  ).to.include.deep.ordered.members(subset);
};

/**
 * ### .notIncludeDeepOrderedMembers(superset, subset, [message])
 *
 * Asserts that `subset` isn't included in `superset` in the same order
 * beginning with the first element in `superset`. Uses a deep equality
 * check.
 *
 *     assert.notIncludeDeepOrderedMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { a: 1 }, { f: 5 } ], 'not include deep ordered members');
 *     assert.notIncludeDeepOrderedMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { b: 2 }, { a: 1 } ], 'not include deep ordered members');
 *     assert.notIncludeDeepOrderedMembers([ { a: 1 }, { b: 2 }, { c: 3 } ], [ { b: 2 }, { c: 3 } ], 'not include deep ordered members');
 *
 * @name notIncludeDeepOrderedMembers
 * @param {Array} superset
 * @param {Array} subset
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.notIncludeDeepOrderedMembers = function (superset, subset, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(
    superset,
    msg,
    assert.notIncludeDeepOrderedMembers,
    true
  ).to.not.include.deep.ordered.members(subset);
};

/**
 * ### .oneOf(inList, list, [message])
 *
 * Asserts that non-object, non-array value `inList` appears in the flat array `list`.
 *
 *     assert.oneOf(1, [ 2, 1 ], 'Not found in list');
 *
 * @name oneOf
 * @param {*} inList
 * @param {Array<*>} list
 * @param {string} msg
 * @namespace Assert
 * @public
 */
assert.oneOf = function (inList, list, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(inList, msg, assert.oneOf, true).to.be.oneOf(list);
};

/**
 * ### isIterable(obj, [message])
 *
 * Asserts that the target is an iterable, which means that it has a iterator
 * with the exception of `String.`
 *
 *     assert.isIterable([1, 2]);
 *
 * @param {unknown} obj
 * @param {string} [msg]
 * @namespace Assert
 * @public
 */
assert.isIterable = function (obj, msg) {
  if (obj == undefined || !obj[Symbol.iterator]) {
    msg = msg
      ? `${msg} expected ${(0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect)(obj)} to be an iterable`
      : `expected ${(0,_utils_index_js__WEBPACK_IMPORTED_MODULE_2__.inspect)(obj)} to be an iterable`;

    throw new assertion_error__WEBPACK_IMPORTED_MODULE_3__.AssertionError(msg, undefined, assert.isIterable);
  }
};

/**
 * ### .changes(function, object, property, [message])
 *
 * Asserts that a function changes the value of a property.
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 22 };
 *     assert.changes(fn, obj, 'val');
 *
 * @name changes
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.changes = function (fn, obj, prop, msg) {
  if (arguments.length === 3 && typeof obj === 'function') {
    msg = prop;
    prop = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.changes, true).to.change(obj, prop);
};

/**
 * ### .changesBy(function, object, property, delta, [message])
 *
 * Asserts that a function changes the value of a property by an amount (delta).
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val += 2 };
 *     assert.changesBy(fn, obj, 'val', 2);
 *
 * @name changesBy
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @param {number} delta msg change amount (delta)
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.changesBy = function (fn, obj, prop, delta, msg) {
  if (arguments.length === 4 && typeof obj === 'function') {
    let tmpMsg = delta;
    delta = prop;
    msg = tmpMsg;
  } else if (arguments.length === 3) {
    delta = prop;
    prop = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.changesBy, true).to.change(obj, prop).by(delta);
};

/**
 * ### .doesNotChange(function, object, property, [message])
 *
 * Asserts that a function does not change the value of a property.
 *
 *   var obj = { val: 10 };
 *   var fn = function() { console.log('foo'); };
 *   assert.doesNotChange(fn, obj, 'val');
 *
 * @name doesNotChange
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @param {string} msg _optional_
 * @returns {unknown}
 * @namespace Assert
 * @public
 */
assert.doesNotChange = function (fn, obj, prop, msg) {
  if (arguments.length === 3 && typeof obj === 'function') {
    msg = prop;
    prop = null;
  }

  return new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.doesNotChange, true).to.not.change(
    obj,
    prop
  );
};

/**
 * ### .changesButNotBy(function, object, property, delta, [message])
 *
 * Asserts that a function does not change the value of a property or of a function's return value by an amount (delta)
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val += 10 };
 *     assert.changesButNotBy(fn, obj, 'val', 5);
 *
 * @name changesButNotBy
 * @param {Function} fn - modifier function
 * @param {object} obj - object or getter function
 * @param {string} prop - property name _optional_
 * @param {number} delta - change amount (delta)
 * @param {string} msg - message _optional_
 * @namespace Assert
 * @public
 */
assert.changesButNotBy = function (fn, obj, prop, delta, msg) {
  if (arguments.length === 4 && typeof obj === 'function') {
    let tmpMsg = delta;
    delta = prop;
    msg = tmpMsg;
  } else if (arguments.length === 3) {
    delta = prop;
    prop = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.changesButNotBy, true).to
    .change(obj, prop)
    .but.not.by(delta);
};

/**
 * ### .increases(function, object, property, [message])
 *
 * Asserts that a function increases a numeric object property.
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 13 };
 *     assert.increases(fn, obj, 'val');
 *
 * @public
 * @namespace Assert
 * @name increases
 * @param {Function} fn - modifier function
 * @param {object} obj - object or getter function
 * @param {string} prop - property name _optional_
 * @param {string} msg - message _optional_
 * @returns {unknown}
 */
assert.increases = function (fn, obj, prop, msg) {
  if (arguments.length === 3 && typeof obj === 'function') {
    msg = prop;
    prop = null;
  }

  return new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.increases, true).to.increase(obj, prop);
};

/**
 * ### .increasesBy(function, object, property, delta, [message])
 *
 * Asserts that a function increases a numeric object property or a function's return value by an amount (delta).
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val += 10 };
 *     assert.increasesBy(fn, obj, 'val', 10);
 *
 * @public
 * @name increasesBy
 * @namespace Assert
 * @param {Function} fn - modifier function
 * @param {object} obj - object or getter function
 * @param {string} prop - property name _optional_
 * @param {number} delta - change amount (delta)
 * @param {string} msg - message _optional_
 */
assert.increasesBy = function (fn, obj, prop, delta, msg) {
  if (arguments.length === 4 && typeof obj === 'function') {
    let tmpMsg = delta;
    delta = prop;
    msg = tmpMsg;
  } else if (arguments.length === 3) {
    delta = prop;
    prop = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.increasesBy, true).to
    .increase(obj, prop)
    .by(delta);
};

/**
 * ### .doesNotIncrease(function, object, property, [message])
 *
 * Asserts that a function does not increase a numeric object property.
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 8 };
 *     assert.doesNotIncrease(fn, obj, 'val');
 *
 * @name doesNotIncrease
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @returns {Assertion}
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.doesNotIncrease = function (fn, obj, prop, msg) {
  if (arguments.length === 3 && typeof obj === 'function') {
    msg = prop;
    prop = null;
  }

  return new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.doesNotIncrease, true).to.not.increase(
    obj,
    prop
  );
};

/**
 * ### .increasesButNotBy(function, object, property, delta, [message])
 *
 * Asserts that a function does not increase a numeric object property or function's return value by an amount (delta).
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 15 };
 *     assert.increasesButNotBy(fn, obj, 'val', 10);
 *
 * @name increasesButNotBy
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @param {number} delta change amount (delta)
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.increasesButNotBy = function (fn, obj, prop, delta, msg) {
  if (arguments.length === 4 && typeof obj === 'function') {
    let tmpMsg = delta;
    delta = prop;
    msg = tmpMsg;
  } else if (arguments.length === 3) {
    delta = prop;
    prop = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.increasesButNotBy, true).to
    .increase(obj, prop)
    .but.not.by(delta);
};

/**
 * ### .decreases(function, object, property, [message])
 *
 * Asserts that a function decreases a numeric object property.
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 5 };
 *     assert.decreases(fn, obj, 'val');
 *
 * @name decreases
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @returns {Assertion}
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.decreases = function (fn, obj, prop, msg) {
  if (arguments.length === 3 && typeof obj === 'function') {
    msg = prop;
    prop = null;
  }

  return new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.decreases, true).to.decrease(obj, prop);
};

/**
 * ### .decreasesBy(function, object, property, delta, [message])
 *
 * Asserts that a function decreases a numeric object property or a function's return value by an amount (delta)
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val -= 5 };
 *     assert.decreasesBy(fn, obj, 'val', 5);
 *
 * @name decreasesBy
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @param {number} delta change amount (delta)
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.decreasesBy = function (fn, obj, prop, delta, msg) {
  if (arguments.length === 4 && typeof obj === 'function') {
    let tmpMsg = delta;
    delta = prop;
    msg = tmpMsg;
  } else if (arguments.length === 3) {
    delta = prop;
    prop = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.decreasesBy, true).to
    .decrease(obj, prop)
    .by(delta);
};

/**
 * ### .doesNotDecrease(function, object, property, [message])
 *
 * Asserts that a function does not decreases a numeric object property.
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 15 };
 *     assert.doesNotDecrease(fn, obj, 'val');
 *
 * @name doesNotDecrease
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @returns {Assertion}
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.doesNotDecrease = function (fn, obj, prop, msg) {
  if (arguments.length === 3 && typeof obj === 'function') {
    msg = prop;
    prop = null;
  }

  return new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.doesNotDecrease, true).to.not.decrease(
    obj,
    prop
  );
};

/**
 * ### .doesNotDecreaseBy(function, object, property, delta, [message])
 *
 * Asserts that a function does not decreases a numeric object property or a function's return value by an amount (delta)
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 5 };
 *     assert.doesNotDecreaseBy(fn, obj, 'val', 1);
 *
 * @name doesNotDecreaseBy
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @param {number} delta change amount (delta)
 * @returns {Assertion}
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.doesNotDecreaseBy = function (fn, obj, prop, delta, msg) {
  if (arguments.length === 4 && typeof obj === 'function') {
    let tmpMsg = delta;
    delta = prop;
    msg = tmpMsg;
  } else if (arguments.length === 3) {
    delta = prop;
    prop = null;
  }

  return new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.doesNotDecreaseBy, true).to.not
    .decrease(obj, prop)
    .by(delta);
};

/**
 * ### .decreasesButNotBy(function, object, property, delta, [message])
 *
 * Asserts that a function does not decreases a numeric object property or a function's return value by an amount (delta)
 *
 *     var obj = { val: 10 };
 *     var fn = function() { obj.val = 5 };
 *     assert.decreasesButNotBy(fn, obj, 'val', 1);
 *
 * @name decreasesButNotBy
 * @param {Function} fn modifier function
 * @param {object} obj object or getter function
 * @param {string} prop property name _optional_
 * @param {number} delta change amount (delta)
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.decreasesButNotBy = function (fn, obj, prop, delta, msg) {
  if (arguments.length === 4 && typeof obj === 'function') {
    let tmpMsg = delta;
    delta = prop;
    msg = tmpMsg;
  } else if (arguments.length === 3) {
    delta = prop;
    prop = null;
  }

  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(fn, msg, assert.decreasesButNotBy, true).to
    .decrease(obj, prop)
    .but.not.by(delta);
};

/**
 * ### .ifError(object)
 *
 * Asserts if value is not a false value, and throws if it is a true value.
 * This is added to allow for chai to be a drop-in replacement for Node's
 * assert class.
 *
 *     var err = new Error('I am a custom error');
 *     assert.ifError(err); // Rethrows err!
 *
 * @name ifError
 * @param {object} val
 * @namespace Assert
 * @public
 */
assert.ifError = function (val) {
  if (val) {
    throw val;
  }
};

/**
 * ### .isExtensible(object)
 *
 * Asserts that `object` is extensible (can have new properties added to it).
 *
 *     assert.isExtensible({});
 *
 * @name isExtensible
 * @alias extensible
 * @param {object} obj
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isExtensible = function (obj, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.isExtensible, true).to.be.extensible;
};

/**
 * ### .isNotExtensible(object)
 *
 * Asserts that `object` is _not_ extensible.
 *
 *     var nonExtensibleObject = Object.preventExtensions({});
 *     var sealedObject = Object.seal({});
 *     var frozenObject = Object.freeze({});
 *
 *     assert.isNotExtensible(nonExtensibleObject);
 *     assert.isNotExtensible(sealedObject);
 *     assert.isNotExtensible(frozenObject);
 *
 * @name isNotExtensible
 * @alias notExtensible
 * @param {object} obj
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isNotExtensible = function (obj, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.isNotExtensible, true).to.not.be.extensible;
};

/**
 * ### .isSealed(object)
 *
 * Asserts that `object` is sealed (cannot have new properties added to it
 * and its existing properties cannot be removed).
 *
 *     var sealedObject = Object.seal({});
 *     var frozenObject = Object.seal({});
 *
 *     assert.isSealed(sealedObject);
 *     assert.isSealed(frozenObject);
 *
 * @name isSealed
 * @alias sealed
 * @param {object} obj
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isSealed = function (obj, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.isSealed, true).to.be.sealed;
};

/**
 * ### .isNotSealed(object)
 *
 * Asserts that `object` is _not_ sealed.
 *
 *     assert.isNotSealed({});
 *
 * @name isNotSealed
 * @alias notSealed
 * @param {object} obj
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isNotSealed = function (obj, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.isNotSealed, true).to.not.be.sealed;
};

/**
 * ### .isFrozen(object)
 *
 * Asserts that `object` is frozen (cannot have new properties added to it
 * and its existing properties cannot be modified).
 *
 *     var frozenObject = Object.freeze({});
 *     assert.frozen(frozenObject);
 *
 * @name isFrozen
 * @alias frozen
 * @param {object} obj
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isFrozen = function (obj, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.isFrozen, true).to.be.frozen;
};

/**
 * ### .isNotFrozen(object)
 *
 * Asserts that `object` is _not_ frozen.
 *
 *     assert.isNotFrozen({});
 *
 * @name isNotFrozen
 * @alias notFrozen
 * @param {object} obj
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isNotFrozen = function (obj, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(obj, msg, assert.isNotFrozen, true).to.not.be.frozen;
};

/**
 * ### .isEmpty(target)
 *
 * Asserts that the target does not contain any values.
 * For arrays and strings, it checks the `length` property.
 * For `Map` and `Set` instances, it checks the `size` property.
 * For non-function objects, it gets the count of own
 * enumerable string keys.
 *
 *     assert.isEmpty([]);
 *     assert.isEmpty('');
 *     assert.isEmpty(new Map);
 *     assert.isEmpty({});
 *
 * @name isEmpty
 * @alias empty
 * @param {object | Array | string | Map | Set} val
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isEmpty = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isEmpty, true).to.be.empty;
};

/**
 * ### .isNotEmpty(target)
 *
 * Asserts that the target contains values.
 * For arrays and strings, it checks the `length` property.
 * For `Map` and `Set` instances, it checks the `size` property.
 * For non-function objects, it gets the count of own
 * enumerable string keys.
 *
 *     assert.isNotEmpty([1, 2]);
 *     assert.isNotEmpty('34');
 *     assert.isNotEmpty(new Set([5, 6]));
 *     assert.isNotEmpty({ key: 7 });
 *
 * @name isNotEmpty
 * @alias notEmpty
 * @param {object | Array | string | Map | Set} val
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.isNotEmpty = function (val, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg, assert.isNotEmpty, true).to.not.be.empty;
};

/**
 * ### .containsSubset(target, subset)
 *
 * Asserts that the target primitive/object/array structure deeply contains all provided fields
 * at the same key/depth as the provided structure.
 *
 * When comparing arrays, the target must contain the subset of at least one of each object/value in the subset array.
 * Order does not matter.
 *
 *     assert.containsSubset(
 *         [{name: {first: "John", last: "Smith"}}, {name: {first: "Jane", last: "Doe"}}],
 *         [{name: {first: "Jane"}}]
 *     );
 *
 * @name containsSubset
 * @alias containSubset
 * @param {unknown} val
 * @param {unknown} exp
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.containsSubset = function (val, exp, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg).to.containSubset(exp);
};

/**
 * ### .doesNotContainSubset(target, subset)
 *
 * The negation of assert.containsSubset.
 *
 * @name doesNotContainSubset
 * @param {unknown} val
 * @param {unknown} exp
 * @param {string} msg _optional_
 * @namespace Assert
 * @public
 */
assert.doesNotContainSubset = function (val, exp, msg) {
  new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, msg).to.not.containSubset(exp);
};

/**
 * Aliases.
 *
 * @param {unknown} name
 * @param {unknown} as
 * @returns {unknown}
 */
const aliases = [
  ['isOk', 'ok'],
  ['isNotOk', 'notOk'],
  ['throws', 'throw'],
  ['throws', 'Throw'],
  ['isExtensible', 'extensible'],
  ['isNotExtensible', 'notExtensible'],
  ['isSealed', 'sealed'],
  ['isNotSealed', 'notSealed'],
  ['isFrozen', 'frozen'],
  ['isNotFrozen', 'notFrozen'],
  ['isEmpty', 'empty'],
  ['isNotEmpty', 'notEmpty'],
  ['isCallable', 'isFunction'],
  ['isNotCallable', 'isNotFunction'],
  ['containsSubset', 'containSubset']
];
for (const [name, as] of aliases) {
  assert[as] = assert[name];
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/interface/expect.js":
/*!********************************************************!*\
  !*** ./node_modules/chai/lib/chai/interface/expect.js ***!
  \********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   expect: () => (/* binding */ expect)
/* harmony export */ });
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../../../index.js */ "./node_modules/chai/index.js");
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var assertion_error__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! assertion-error */ "./node_modules/assertion-error/index.js");
/*!
 * chai
 * Copyright(c) 2011-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */





/**
 * @param {unknown} val
 * @param {string} message
 * @returns {Assertion}
 */
function expect(val, message) {
  return new _assertion_js__WEBPACK_IMPORTED_MODULE_1__.Assertion(val, message);
}



/**
 * ### .fail([message])
 * ### .fail(actual, expected, [message], [operator])
 *
 * Throw a failure.
 *
 *     expect.fail();
 *     expect.fail("custom error message");
 *     expect.fail(1, 2);
 *     expect.fail(1, 2, "custom error message");
 *     expect.fail(1, 2, "custom error message", ">");
 *     expect.fail(1, 2, undefined, ">");
 *
 * @name fail
 * @param {unknown} actual
 * @param {unknown} expected
 * @param {string} message
 * @param {string} operator
 * @namespace expect
 * @public
 */
expect.fail = function (actual, expected, message, operator) {
  if (arguments.length < 2) {
    message = actual;
    actual = undefined;
  }

  message = message || 'expect.fail()';
  throw new assertion_error__WEBPACK_IMPORTED_MODULE_2__.AssertionError(
    message,
    {
      actual: actual,
      expected: expected,
      operator: operator
    },
    _index_js__WEBPACK_IMPORTED_MODULE_0__.expect.fail
  );
};


/***/ }),

/***/ "./node_modules/chai/lib/chai/interface/should.js":
/*!********************************************************!*\
  !*** ./node_modules/chai/lib/chai/interface/should.js ***!
  \********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   Should: () => (/* binding */ Should),
/* harmony export */   should: () => (/* binding */ should)
/* harmony export */ });
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var assertion_error__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! assertion-error */ "./node_modules/assertion-error/index.js");
/*!
 * chai
 * Copyright(c) 2011-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */




/**
 * @returns {void}
 */
function loadShould() {
  // explicitly define this method as function as to have it's name to include as `ssfi`
  /**
   * @returns {Assertion}
   */
  function shouldGetter() {
    if (
      this instanceof String ||
      this instanceof Number ||
      this instanceof Boolean ||
      (typeof Symbol === 'function' && this instanceof Symbol) ||
      (typeof BigInt === 'function' && this instanceof BigInt)
    ) {
      return new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(this.valueOf(), null, shouldGetter);
    }
    return new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(this, null, shouldGetter);
  }
  /**
   * @param {unknown} value
   */
  function shouldSetter(value) {
    // See https://github.com/chaijs/chai/issues/86: this makes
    // `whatever.should = someValue` actually set `someValue`, which is
    // especially useful for `global.should = require('chai').should()`.
    //
    // Note that we have to use [[DefineProperty]] instead of [[Put]]
    // since otherwise we would trigger this very setter!
    Object.defineProperty(this, 'should', {
      value: value,
      enumerable: true,
      configurable: true,
      writable: true
    });
  }
  // modify Object.prototype to have `should`
  Object.defineProperty(Object.prototype, 'should', {
    set: shouldSetter,
    get: shouldGetter,
    configurable: true
  });

  let should = {};

  /**
   * ### .fail([message])
   * ### .fail(actual, expected, [message], [operator])
   *
   * Throw a failure.
   *
   *     should.fail();
   *     should.fail("custom error message");
   *     should.fail(1, 2);
   *     should.fail(1, 2, "custom error message");
   *     should.fail(1, 2, "custom error message", ">");
   *     should.fail(1, 2, undefined, ">");
   *
   * @name fail
   * @param {unknown} actual
   * @param {unknown} expected
   * @param {string} message
   * @param {string} operator
   * @namespace BDD
   * @public
   */
  should.fail = function (actual, expected, message, operator) {
    if (arguments.length < 2) {
      message = actual;
      actual = undefined;
    }

    message = message || 'should.fail()';
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_1__.AssertionError(
      message,
      {
        actual: actual,
        expected: expected,
        operator: operator
      },
      should.fail
    );
  };

  /**
   * ### .equal(actual, expected, [message])
   *
   * Asserts non-strict equality (`==`) of `actual` and `expected`.
   *
   *     should.equal(3, '3', '== coerces values to strings');
   *
   * @name equal
   * @param {unknown} actual
   * @param {unknown} expected
   * @param {string} message
   * @namespace Should
   * @public
   */
  should.equal = function (actual, expected, message) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(actual, message).to.equal(expected);
  };

  /**
   * ### .throw(function, [constructor/string/regexp], [string/regexp], [message])
   *
   * Asserts that `function` will throw an error that is an instance of
   * `constructor`, or alternately that it will throw an error with message
   * matching `regexp`.
   *
   *     should.throw(fn, 'function throws a reference error');
   *     should.throw(fn, /function throws a reference error/);
   *     should.throw(fn, ReferenceError);
   *     should.throw(fn, ReferenceError, 'function throws a reference error');
   *     should.throw(fn, ReferenceError, /function throws a reference error/);
   *
   * @name throw
   * @alias Throw
   * @param {Function} fn
   * @param {Error} errt
   * @param {RegExp} errs
   * @param {string} msg
   * @see https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error#Error_types
   * @namespace Should
   * @public
   */
  should.Throw = function (fn, errt, errs, msg) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(fn, msg).to.Throw(errt, errs);
  };

  /**
   * ### .exist
   *
   * Asserts that the target is neither `null` nor `undefined`.
   *
   *     var foo = 'hi';
   *     should.exist(foo, 'foo exists');
   *
   * @param {unknown} val
   * @param {string} msg
   * @name exist
   * @namespace Should
   * @public
   */
  should.exist = function (val, msg) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(val, msg).to.exist;
  };

  // negation
  should.not = {};

  /**
   * ### .not.equal(actual, expected, [message])
   *
   * Asserts non-strict inequality (`!=`) of `actual` and `expected`.
   *
   *     should.not.equal(3, 4, 'these numbers are not equal');
   *
   * @name not.equal
   * @param {unknown} actual
   * @param {unknown} expected
   * @param {string} msg
   * @namespace Should
   * @public
   */
  should.not.equal = function (actual, expected, msg) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(actual, msg).to.not.equal(expected);
  };

  /**
   * ### .throw(function, [constructor/regexp], [message])
   *
   * Asserts that `function` will _not_ throw an error that is an instance of
   * `constructor`, or alternately that it will not throw an error with message
   * matching `regexp`.
   *
   *     should.not.throw(fn, Error, 'function does not throw');
   *
   * @name not.throw
   * @alias not.Throw
   * @param {Function} fn
   * @param {Error} errt
   * @param {RegExp} errs
   * @param {string} msg
   * @see https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error#Error_types
   * @namespace Should
   * @public
   */
  should.not.Throw = function (fn, errt, errs, msg) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(fn, msg).to.not.Throw(errt, errs);
  };

  /**
   * ### .not.exist
   *
   * Asserts that the target is neither `null` nor `undefined`.
   *
   *     var bar = null;
   *     should.not.exist(bar, 'bar does not exist');
   *
   * @namespace Should
   * @name not.exist
   * @param {unknown} val
   * @param {string} msg
   * @public
   */
  should.not.exist = function (val, msg) {
    new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion(val, msg).to.not.exist;
  };

  should['throw'] = should['Throw'];
  should.not['throw'] = should.not['Throw'];

  return should;
}

const should = loadShould;
const Should = loadShould;


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/addChainableMethod.js":
/*!****************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/addChainableMethod.js ***!
  \****************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   addChainableMethod: () => (/* binding */ addChainableMethod)
/* harmony export */ });
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var _addLengthGuard_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./addLengthGuard.js */ "./node_modules/chai/lib/chai/utils/addLengthGuard.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _proxify_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./proxify.js */ "./node_modules/chai/lib/chai/utils/proxify.js");
/* harmony import */ var _transferFlags_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./transferFlags.js */ "./node_modules/chai/lib/chai/utils/transferFlags.js");
/*!
 * Chai - addChainingMethod utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */







/**
 * Module variables
 */

// Check whether `Object.setPrototypeOf` is supported
let canSetPrototype = typeof Object.setPrototypeOf === 'function';

// Without `Object.setPrototypeOf` support, this module will need to add properties to a function.
// However, some of functions' own props are not configurable and should be skipped.
let testFn = function () {};
let excludeNames = Object.getOwnPropertyNames(testFn).filter(function (name) {
  let propDesc = Object.getOwnPropertyDescriptor(testFn, name);

  // Note: PhantomJS 1.x includes `callee` as one of `testFn`'s own properties,
  // but then returns `undefined` as the property descriptor for `callee`. As a
  // workaround, we perform an otherwise unnecessary type-check for `propDesc`,
  // and then filter it out if it's not an object as it should be.
  if (typeof propDesc !== 'object') return true;

  return !propDesc.configurable;
});

// Cache `Function` properties
let call = Function.prototype.call,
  apply = Function.prototype.apply;

/**
 * ### .addChainableMethod(ctx, name, method, chainingBehavior)
 *
 * Adds a method to an object, such that the method can also be chained.
 *
 *     utils.addChainableMethod(chai.Assertion.prototype, 'foo', function (str) {
 *         var obj = utils.flag(this, 'object');
 *         new chai.Assertion(obj).to.be.equal(str);
 *     });
 *
 * Can also be accessed directly from `chai.Assertion`.
 *
 *     chai.Assertion.addChainableMethod('foo', fn, chainingBehavior);
 *
 * The result can then be used as both a method assertion, executing both `method` and
 * `chainingBehavior`, or as a language chain, which only executes `chainingBehavior`.
 *
 *     expect(fooStr).to.be.foo('bar');
 *     expect(fooStr).to.be.foo.equal('foo');
 *
 * @param {object} ctx object to which the method is added
 * @param {string} name of method to add
 * @param {Function} method function to be used for `name`, when called
 * @param {Function} chainingBehavior function to be called every time the property is accessed
 * @namespace Utils
 * @name addChainableMethod
 * @public
 */
function addChainableMethod(ctx, name, method, chainingBehavior) {
  if (typeof chainingBehavior !== 'function') {
    chainingBehavior = function () {};
  }

  let chainableBehavior = {
    method: method,
    chainingBehavior: chainingBehavior
  };

  // save the methods so we can overwrite them later, if we need to.
  if (!ctx.__methods) {
    ctx.__methods = {};
  }
  ctx.__methods[name] = chainableBehavior;

  Object.defineProperty(ctx, name, {
    get: function chainableMethodGetter() {
      chainableBehavior.chainingBehavior.call(this);

      let chainableMethodWrapper = function () {
        // Setting the `ssfi` flag to `chainableMethodWrapper` causes this
        // function to be the starting point for removing implementation
        // frames from the stack trace of a failed assertion.
        //
        // However, we only want to use this function as the starting point if
        // the `lockSsfi` flag isn't set.
        //
        // If the `lockSsfi` flag is set, then this assertion is being
        // invoked from inside of another assertion. In this case, the `ssfi`
        // flag has already been set by the outer assertion.
        //
        // Note that overwriting a chainable method merely replaces the saved
        // methods in `ctx.__methods` instead of completely replacing the
        // overwritten assertion. Therefore, an overwriting assertion won't
        // set the `ssfi` or `lockSsfi` flags.
        if (!(0,_flag_js__WEBPACK_IMPORTED_MODULE_2__.flag)(this, 'lockSsfi')) {
          (0,_flag_js__WEBPACK_IMPORTED_MODULE_2__.flag)(this, 'ssfi', chainableMethodWrapper);
        }

        let result = chainableBehavior.method.apply(this, arguments);
        if (result !== undefined) {
          return result;
        }

        let newAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion();
        (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_4__.transferFlags)(this, newAssertion);
        return newAssertion;
      };

      (0,_addLengthGuard_js__WEBPACK_IMPORTED_MODULE_1__.addLengthGuard)(chainableMethodWrapper, name, true);

      // Use `Object.setPrototypeOf` if available
      if (canSetPrototype) {
        // Inherit all properties from the object by replacing the `Function` prototype
        let prototype = Object.create(this);
        // Restore the `call` and `apply` methods from `Function`
        prototype.call = call;
        prototype.apply = apply;
        Object.setPrototypeOf(chainableMethodWrapper, prototype);
      }
      // Otherwise, redefine all properties (slow!)
      else {
        let asserterNames = Object.getOwnPropertyNames(ctx);
        asserterNames.forEach(function (asserterName) {
          if (excludeNames.indexOf(asserterName) !== -1) {
            return;
          }

          let pd = Object.getOwnPropertyDescriptor(ctx, asserterName);
          Object.defineProperty(chainableMethodWrapper, asserterName, pd);
        });
      }

      (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_4__.transferFlags)(this, chainableMethodWrapper);
      return (0,_proxify_js__WEBPACK_IMPORTED_MODULE_3__.proxify)(chainableMethodWrapper);
    },
    configurable: true
  });
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/addLengthGuard.js":
/*!************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/addLengthGuard.js ***!
  \************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   addLengthGuard: () => (/* binding */ addLengthGuard)
/* harmony export */ });
const fnLengthDesc = Object.getOwnPropertyDescriptor(function () {}, 'length');

/*!
 * Chai - addLengthGuard utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/**
 * ### .addLengthGuard(fn, assertionName, isChainable)
 *
 * Define `length` as a getter on the given uninvoked method assertion. The
 * getter acts as a guard against chaining `length` directly off of an uninvoked
 * method assertion, which is a problem because it references `function`'s
 * built-in `length` property instead of Chai's `length` assertion. When the
 * getter catches the user making this mistake, it throws an error with a
 * helpful message.
 *
 * There are two ways in which this mistake can be made. The first way is by
 * chaining the `length` assertion directly off of an uninvoked chainable
 * method. In this case, Chai suggests that the user use `lengthOf` instead. The
 * second way is by chaining the `length` assertion directly off of an uninvoked
 * non-chainable method. Non-chainable methods must be invoked prior to
 * chaining. In this case, Chai suggests that the user consult the docs for the
 * given assertion.
 *
 * If the `length` property of functions is unconfigurable, then return `fn`
 * without modification.
 *
 * Note that in ES6, the function's `length` property is configurable, so once
 * support for legacy environments is dropped, Chai's `length` property can
 * replace the built-in function's `length` property, and this length guard will
 * no longer be necessary. In the mean time, maintaining consistency across all
 * environments is the priority.
 *
 * @param {Function} fn
 * @param {string} assertionName
 * @param {boolean} isChainable
 * @returns {unknown}
 * @namespace Utils
 * @name addLengthGuard
 */
function addLengthGuard(fn, assertionName, isChainable) {
  if (!fnLengthDesc.configurable) return fn;

  Object.defineProperty(fn, 'length', {
    get: function () {
      if (isChainable) {
        throw Error(
          'Invalid Chai property: ' +
            assertionName +
            '.length. Due' +
            ' to a compatibility issue, "length" cannot directly follow "' +
            assertionName +
            '". Use "' +
            assertionName +
            '.lengthOf" instead.'
        );
      }

      throw Error(
        'Invalid Chai property: ' +
          assertionName +
          '.length. See' +
          ' docs for proper usage of "' +
          assertionName +
          '".'
      );
    }
  });

  return fn;
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/addMethod.js":
/*!*******************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/addMethod.js ***!
  \*******************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   addMethod: () => (/* binding */ addMethod)
/* harmony export */ });
/* harmony import */ var _addLengthGuard_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./addLengthGuard.js */ "./node_modules/chai/lib/chai/utils/addLengthGuard.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _proxify_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./proxify.js */ "./node_modules/chai/lib/chai/utils/proxify.js");
/* harmony import */ var _transferFlags_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./transferFlags.js */ "./node_modules/chai/lib/chai/utils/transferFlags.js");
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/*!
 * Chai - addMethod utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */







/**
 * ### .addMethod(ctx, name, method)
 *
 * Adds a method to the prototype of an object.
 *
 *     utils.addMethod(chai.Assertion.prototype, 'foo', function (str) {
 *         var obj = utils.flag(this, 'object');
 *         new chai.Assertion(obj).to.be.equal(str);
 *     });
 *
 * Can also be accessed directly from `chai.Assertion`.
 *
 *     chai.Assertion.addMethod('foo', fn);
 *
 * Then can be used as any other assertion.
 *
 *     expect(fooStr).to.be.foo('bar');
 *
 * @param {object} ctx object to which the method is added
 * @param {string} name of method to add
 * @param {Function} method function to be used for name
 * @namespace Utils
 * @name addMethod
 * @public
 */
function addMethod(ctx, name, method) {
  let methodWrapper = function () {
    // Setting the `ssfi` flag to `methodWrapper` causes this function to be the
    // starting point for removing implementation frames from the stack trace of
    // a failed assertion.
    //
    // However, we only want to use this function as the starting point if the
    // `lockSsfi` flag isn't set.
    //
    // If the `lockSsfi` flag is set, then either this assertion has been
    // overwritten by another assertion, or this assertion is being invoked from
    // inside of another assertion. In the first case, the `ssfi` flag has
    // already been set by the overwriting assertion. In the second case, the
    // `ssfi` flag has already been set by the outer assertion.
    if (!(0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'lockSsfi')) {
      (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'ssfi', methodWrapper);
    }

    let result = method.apply(this, arguments);
    if (result !== undefined) return result;

    let newAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_4__.Assertion();
    (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_3__.transferFlags)(this, newAssertion);
    return newAssertion;
  };

  (0,_addLengthGuard_js__WEBPACK_IMPORTED_MODULE_0__.addLengthGuard)(methodWrapper, name, false);
  ctx[name] = (0,_proxify_js__WEBPACK_IMPORTED_MODULE_2__.proxify)(methodWrapper, name);
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/addProperty.js":
/*!*********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/addProperty.js ***!
  \*********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   addProperty: () => (/* binding */ addProperty)
/* harmony export */ });
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./isProxyEnabled.js */ "./node_modules/chai/lib/chai/utils/isProxyEnabled.js");
/* harmony import */ var _transferFlags_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./transferFlags.js */ "./node_modules/chai/lib/chai/utils/transferFlags.js");
/*!
 * Chai - addProperty utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */






/**
 * ### .addProperty(ctx, name, getter)
 *
 * Adds a property to the prototype of an object.
 *
 *     utils.addProperty(chai.Assertion.prototype, 'foo', function () {
 *         var obj = utils.flag(this, 'object');
 *         new chai.Assertion(obj).to.be.instanceof(Foo);
 *     });
 *
 * Can also be accessed directly from `chai.Assertion`.
 *
 *     chai.Assertion.addProperty('foo', fn);
 *
 * Then can be used as any other assertion.
 *
 *     expect(myFoo).to.be.foo;
 *
 * @param {object} ctx object to which the property is added
 * @param {string} name of property to add
 * @param {Function} getter function to be used for name
 * @namespace Utils
 * @name addProperty
 * @public
 */
function addProperty(ctx, name, getter) {
  getter = getter === undefined ? function () {} : getter;

  Object.defineProperty(ctx, name, {
    get: function propertyGetter() {
      // Setting the `ssfi` flag to `propertyGetter` causes this function to
      // be the starting point for removing implementation frames from the
      // stack trace of a failed assertion.
      //
      // However, we only want to use this function as the starting point if
      // the `lockSsfi` flag isn't set and proxy protection is disabled.
      //
      // If the `lockSsfi` flag is set, then either this assertion has been
      // overwritten by another assertion, or this assertion is being invoked
      // from inside of another assertion. In the first case, the `ssfi` flag
      // has already been set by the overwriting assertion. In the second
      // case, the `ssfi` flag has already been set by the outer assertion.
      //
      // If proxy protection is enabled, then the `ssfi` flag has already been
      // set by the proxy getter.
      if (!(0,_isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_2__.isProxyEnabled)() && !(0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'lockSsfi')) {
        (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'ssfi', propertyGetter);
      }

      let result = getter.call(this);
      if (result !== undefined) return result;

      let newAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion();
      (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_3__.transferFlags)(this, newAssertion);
      return newAssertion;
    },
    configurable: true
  });
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/compareByInspect.js":
/*!**************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/compareByInspect.js ***!
  \**************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   compareByInspect: () => (/* binding */ compareByInspect)
/* harmony export */ });
/* harmony import */ var _inspect_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./inspect.js */ "./node_modules/chai/lib/chai/utils/inspect.js");
/*!
 * Chai - compareByInspect utility
 * Copyright(c) 2011-2016 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */



/**
 * ### .compareByInspect(mixed, mixed)
 *
 * To be used as a compareFunction with Array.prototype.sort. Compares elements
 * using inspect instead of default behavior of using toString so that Symbols
 * and objects with irregular/missing toString can still be sorted without a
 * TypeError.
 *
 * @param {unknown} a first element to compare
 * @param {unknown} b second element to compare
 * @returns {number} -1 if 'a' should come before 'b'; otherwise 1
 * @name compareByInspect
 * @namespace Utils
 * @public
 */
function compareByInspect(a, b) {
  return (0,_inspect_js__WEBPACK_IMPORTED_MODULE_0__.inspect)(a) < (0,_inspect_js__WEBPACK_IMPORTED_MODULE_0__.inspect)(b) ? -1 : 1;
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/expectTypes.js":
/*!*********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/expectTypes.js ***!
  \*********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   expectTypes: () => (/* binding */ expectTypes)
/* harmony export */ });
/* harmony import */ var assertion_error__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! assertion-error */ "./node_modules/assertion-error/index.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _type_detect_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./type-detect.js */ "./node_modules/chai/lib/chai/utils/type-detect.js");
/*!
 * Chai - expectTypes utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */





/**
 * ### .expectTypes(obj, types)
 *
 * Ensures that the object being tested against is of a valid type.
 *
 *     utils.expectTypes(this, ['array', 'object', 'string']);
 *
 * @param {unknown} obj constructed Assertion
 * @param {Array} types A list of allowed types for this assertion
 * @namespace Utils
 * @name expectTypes
 * @public
 */
function expectTypes(obj, types) {
  let flagMsg = (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(obj, 'message');
  let ssfi = (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(obj, 'ssfi');

  flagMsg = flagMsg ? flagMsg + ': ' : '';

  obj = (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(obj, 'object');
  types = types.map(function (t) {
    return t.toLowerCase();
  });
  types.sort();

  // Transforms ['lorem', 'ipsum'] into 'a lorem, or an ipsum'
  let str = types
    .map(function (t, index) {
      let art = ~['a', 'e', 'i', 'o', 'u'].indexOf(t.charAt(0)) ? 'an' : 'a';
      let or = types.length > 1 && index === types.length - 1 ? 'or ' : '';
      return or + art + ' ' + t;
    })
    .join(', ');

  let objType = (0,_type_detect_js__WEBPACK_IMPORTED_MODULE_2__.type)(obj).toLowerCase();

  if (
    !types.some(function (expected) {
      return objType === expected;
    })
  ) {
    throw new assertion_error__WEBPACK_IMPORTED_MODULE_0__.AssertionError(
      flagMsg + 'object tested must be ' + str + ', but ' + objType + ' given',
      undefined,
      ssfi
    );
  }
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/flag.js":
/*!**************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/flag.js ***!
  \**************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   flag: () => (/* binding */ flag)
/* harmony export */ });
/*!
 * Chai - flag utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/**
 * ### .flag(object, key, [value])
 *
 * Get or set a flag value on an object. If a
 * value is provided it will be set, else it will
 * return the currently set value or `undefined` if
 * the value is not set.
 *
 *     utils.flag(this, 'foo', 'bar'); // setter
 *     utils.flag(this, 'foo'); // getter, returns `bar`
 *
 * @template {{__flags?: {[key: PropertyKey]: unknown}}} T
 * @param {T} obj object constructed Assertion
 * @param {string} key
 * @param {unknown} [value]
 * @namespace Utils
 * @name flag
 * @returns {unknown | undefined}
 * @private
 */
function flag(obj, key, value) {
  let flags = obj.__flags || (obj.__flags = Object.create(null));
  if (arguments.length === 3) {
    flags[key] = value;
  } else {
    return flags[key];
  }
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/getActual.js":
/*!*******************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/getActual.js ***!
  \*******************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   getActual: () => (/* binding */ getActual)
/* harmony export */ });
/*!
 * Chai - getActual utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/**
 * ### .getActual(object, [actual])
 *
 * Returns the `actual` value for an Assertion.
 *
 * @param {object} obj object (constructed Assertion)
 * @param {unknown} args chai.Assertion.prototype.assert arguments
 * @returns {unknown}
 * @namespace Utils
 * @name getActual
 */
function getActual(obj, args) {
  return args.length > 4 ? args[4] : obj._obj;
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/getMessage.js":
/*!********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/getMessage.js ***!
  \********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   getMessage: () => (/* binding */ getMessage)
/* harmony export */ });
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _getActual_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./getActual.js */ "./node_modules/chai/lib/chai/utils/getActual.js");
/* harmony import */ var _objDisplay_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./objDisplay.js */ "./node_modules/chai/lib/chai/utils/objDisplay.js");
/*!
 * Chai - message composition utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */





/**
 * ### .getMessage(object, message, negateMessage)
 *
 * Construct the error message based on flags
 * and template tags. Template tags will return
 * a stringified inspection of the object referenced.
 *
 * Message template tags:
 * - `#{this}` current asserted object
 * - `#{act}` actual value
 * - `#{exp}` expected value
 *
 * @param {object} obj object (constructed Assertion)
 * @param {IArguments} args chai.Assertion.prototype.assert arguments
 * @returns {string}
 * @namespace Utils
 * @name getMessage
 * @public
 */
function getMessage(obj, args) {
  let negate = (0,_flag_js__WEBPACK_IMPORTED_MODULE_0__.flag)(obj, 'negate');
  let val = (0,_flag_js__WEBPACK_IMPORTED_MODULE_0__.flag)(obj, 'object');
  let expected = args[3];
  let actual = (0,_getActual_js__WEBPACK_IMPORTED_MODULE_1__.getActual)(obj, args);
  let msg = negate ? args[2] : args[1];
  let flagMsg = (0,_flag_js__WEBPACK_IMPORTED_MODULE_0__.flag)(obj, 'message');

  if (typeof msg === 'function') msg = msg();
  msg = msg || '';
  msg = msg
    .replace(/#\{this\}/g, function () {
      return (0,_objDisplay_js__WEBPACK_IMPORTED_MODULE_2__.objDisplay)(val);
    })
    .replace(/#\{act\}/g, function () {
      return (0,_objDisplay_js__WEBPACK_IMPORTED_MODULE_2__.objDisplay)(actual);
    })
    .replace(/#\{exp\}/g, function () {
      return (0,_objDisplay_js__WEBPACK_IMPORTED_MODULE_2__.objDisplay)(expected);
    });

  return flagMsg ? flagMsg + ': ' + msg : msg;
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/getOperator.js":
/*!*********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/getOperator.js ***!
  \*********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   getOperator: () => (/* binding */ getOperator)
/* harmony export */ });
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _type_detect_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./type-detect.js */ "./node_modules/chai/lib/chai/utils/type-detect.js");



/**
 * @param {unknown} obj
 * @returns {boolean}
 */
function isObjectType(obj) {
  let objectType = (0,_type_detect_js__WEBPACK_IMPORTED_MODULE_1__.type)(obj);
  let objectTypes = ['Array', 'Object', 'Function'];

  return objectTypes.indexOf(objectType) !== -1;
}

/**
 * ### .getOperator(message)
 *
 * Extract the operator from error message.
 * Operator defined is based on below link
 * https://nodejs.org/api/assert.html#assert_assert.
 *
 * Returns the `operator` or `undefined` value for an Assertion.
 *
 * @param {object} obj object (constructed Assertion)
 * @param {unknown} args chai.Assertion.prototype.assert arguments
 * @returns {unknown}
 * @namespace Utils
 * @name getOperator
 * @public
 */
function getOperator(obj, args) {
  let operator = (0,_flag_js__WEBPACK_IMPORTED_MODULE_0__.flag)(obj, 'operator');
  let negate = (0,_flag_js__WEBPACK_IMPORTED_MODULE_0__.flag)(obj, 'negate');
  let expected = args[3];
  let msg = negate ? args[2] : args[1];

  if (operator) {
    return operator;
  }

  if (typeof msg === 'function') msg = msg();

  msg = msg || '';
  if (!msg) {
    return undefined;
  }

  if (/\shave\s/.test(msg)) {
    return undefined;
  }

  let isObject = isObjectType(expected);
  if (/\snot\s/.test(msg)) {
    return isObject ? 'notDeepStrictEqual' : 'notStrictEqual';
  }

  return isObject ? 'deepStrictEqual' : 'strictEqual';
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/getOwnEnumerableProperties.js":
/*!************************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/getOwnEnumerableProperties.js ***!
  \************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   getOwnEnumerableProperties: () => (/* binding */ getOwnEnumerableProperties)
/* harmony export */ });
/* harmony import */ var _getOwnEnumerablePropertySymbols_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./getOwnEnumerablePropertySymbols.js */ "./node_modules/chai/lib/chai/utils/getOwnEnumerablePropertySymbols.js");
/*!
 * Chai - getOwnEnumerableProperties utility
 * Copyright(c) 2011-2016 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */



/**
 * ### .getOwnEnumerableProperties(object)
 *
 * This allows the retrieval of directly-owned enumerable property names and
 * symbols of an object. This function is necessary because Object.keys only
 * returns enumerable property names, not enumerable property symbols.
 *
 * @param {object} obj
 * @returns {Array}
 * @namespace Utils
 * @name getOwnEnumerableProperties
 * @public
 */
function getOwnEnumerableProperties(obj) {
  return Object.keys(obj).concat((0,_getOwnEnumerablePropertySymbols_js__WEBPACK_IMPORTED_MODULE_0__.getOwnEnumerablePropertySymbols)(obj));
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/getOwnEnumerablePropertySymbols.js":
/*!*****************************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/getOwnEnumerablePropertySymbols.js ***!
  \*****************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   getOwnEnumerablePropertySymbols: () => (/* binding */ getOwnEnumerablePropertySymbols)
/* harmony export */ });
/*!
 * Chai - getOwnEnumerablePropertySymbols utility
 * Copyright(c) 2011-2016 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/**
 * ### .getOwnEnumerablePropertySymbols(object)
 *
 * This allows the retrieval of directly-owned enumerable property symbols of an
 * object. This function is necessary because Object.getOwnPropertySymbols
 * returns both enumerable and non-enumerable property symbols.
 *
 * @param {object} obj
 * @returns {Array}
 * @namespace Utils
 * @name getOwnEnumerablePropertySymbols
 * @public
 */
function getOwnEnumerablePropertySymbols(obj) {
  if (typeof Object.getOwnPropertySymbols !== 'function') return [];

  return Object.getOwnPropertySymbols(obj).filter(function (sym) {
    return Object.getOwnPropertyDescriptor(obj, sym).enumerable;
  });
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/getProperties.js":
/*!***********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/getProperties.js ***!
  \***********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   getProperties: () => (/* binding */ getProperties)
/* harmony export */ });
/*!
 * Chai - getProperties utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/**
 * ### .getProperties(object)
 *
 * This allows the retrieval of property names of an object, enumerable or not,
 * inherited or not.
 *
 * @param {object} object
 * @returns {Array}
 * @namespace Utils
 * @name getProperties
 * @public
 */
function getProperties(object) {
  let result = Object.getOwnPropertyNames(object);

  /**
   * @param {unknown} property
   */
  function addProperty(property) {
    if (result.indexOf(property) === -1) {
      result.push(property);
    }
  }

  let proto = Object.getPrototypeOf(object);
  while (proto !== null) {
    Object.getOwnPropertyNames(proto).forEach(addProperty);
    proto = Object.getPrototypeOf(proto);
  }

  return result;
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/index.js":
/*!***************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/index.js ***!
  \***************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   addChainableMethod: () => (/* reexport safe */ _addChainableMethod_js__WEBPACK_IMPORTED_MODULE_16__.addChainableMethod),
/* harmony export */   addLengthGuard: () => (/* reexport safe */ _addLengthGuard_js__WEBPACK_IMPORTED_MODULE_22__.addLengthGuard),
/* harmony export */   addMethod: () => (/* reexport safe */ _addMethod_js__WEBPACK_IMPORTED_MODULE_13__.addMethod),
/* harmony export */   addProperty: () => (/* reexport safe */ _addProperty_js__WEBPACK_IMPORTED_MODULE_12__.addProperty),
/* harmony export */   checkError: () => (/* reexport module object */ check_error__WEBPACK_IMPORTED_MODULE_0__),
/* harmony export */   compareByInspect: () => (/* reexport safe */ _compareByInspect_js__WEBPACK_IMPORTED_MODULE_18__.compareByInspect),
/* harmony export */   eql: () => (/* reexport safe */ deep_eql__WEBPACK_IMPORTED_MODULE_10__["default"]),
/* harmony export */   expectTypes: () => (/* reexport safe */ _expectTypes_js__WEBPACK_IMPORTED_MODULE_3__.expectTypes),
/* harmony export */   flag: () => (/* reexport safe */ _flag_js__WEBPACK_IMPORTED_MODULE_8__.flag),
/* harmony export */   getActual: () => (/* reexport safe */ _getActual_js__WEBPACK_IMPORTED_MODULE_5__.getActual),
/* harmony export */   getMessage: () => (/* reexport safe */ _getMessage_js__WEBPACK_IMPORTED_MODULE_4__.getMessage),
/* harmony export */   getName: () => (/* binding */ getName),
/* harmony export */   getOperator: () => (/* reexport safe */ _getOperator_js__WEBPACK_IMPORTED_MODULE_25__.getOperator),
/* harmony export */   getOwnEnumerableProperties: () => (/* reexport safe */ _getOwnEnumerableProperties_js__WEBPACK_IMPORTED_MODULE_20__.getOwnEnumerableProperties),
/* harmony export */   getOwnEnumerablePropertySymbols: () => (/* reexport safe */ _getOwnEnumerablePropertySymbols_js__WEBPACK_IMPORTED_MODULE_19__.getOwnEnumerablePropertySymbols),
/* harmony export */   getPathInfo: () => (/* reexport safe */ pathval__WEBPACK_IMPORTED_MODULE_11__.getPathInfo),
/* harmony export */   hasProperty: () => (/* reexport safe */ pathval__WEBPACK_IMPORTED_MODULE_11__.hasProperty),
/* harmony export */   inspect: () => (/* reexport safe */ _inspect_js__WEBPACK_IMPORTED_MODULE_6__.inspect),
/* harmony export */   isNaN: () => (/* reexport safe */ _isNaN_js__WEBPACK_IMPORTED_MODULE_24__.isNaN),
/* harmony export */   isNumeric: () => (/* binding */ isNumeric),
/* harmony export */   isProxyEnabled: () => (/* reexport safe */ _isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_23__.isProxyEnabled),
/* harmony export */   isRegExp: () => (/* binding */ isRegExp),
/* harmony export */   objDisplay: () => (/* reexport safe */ _objDisplay_js__WEBPACK_IMPORTED_MODULE_7__.objDisplay),
/* harmony export */   overwriteChainableMethod: () => (/* reexport safe */ _overwriteChainableMethod_js__WEBPACK_IMPORTED_MODULE_17__.overwriteChainableMethod),
/* harmony export */   overwriteMethod: () => (/* reexport safe */ _overwriteMethod_js__WEBPACK_IMPORTED_MODULE_15__.overwriteMethod),
/* harmony export */   overwriteProperty: () => (/* reexport safe */ _overwriteProperty_js__WEBPACK_IMPORTED_MODULE_14__.overwriteProperty),
/* harmony export */   proxify: () => (/* reexport safe */ _proxify_js__WEBPACK_IMPORTED_MODULE_21__.proxify),
/* harmony export */   test: () => (/* reexport safe */ _test_js__WEBPACK_IMPORTED_MODULE_1__.test),
/* harmony export */   transferFlags: () => (/* reexport safe */ _transferFlags_js__WEBPACK_IMPORTED_MODULE_9__.transferFlags),
/* harmony export */   type: () => (/* reexport safe */ _type_detect_js__WEBPACK_IMPORTED_MODULE_2__.type)
/* harmony export */ });
/* harmony import */ var check_error__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! check-error */ "./node_modules/check-error/index.js");
/* harmony import */ var _test_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./test.js */ "./node_modules/chai/lib/chai/utils/test.js");
/* harmony import */ var _type_detect_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./type-detect.js */ "./node_modules/chai/lib/chai/utils/type-detect.js");
/* harmony import */ var _expectTypes_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./expectTypes.js */ "./node_modules/chai/lib/chai/utils/expectTypes.js");
/* harmony import */ var _getMessage_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./getMessage.js */ "./node_modules/chai/lib/chai/utils/getMessage.js");
/* harmony import */ var _getActual_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ./getActual.js */ "./node_modules/chai/lib/chai/utils/getActual.js");
/* harmony import */ var _inspect_js__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ./inspect.js */ "./node_modules/chai/lib/chai/utils/inspect.js");
/* harmony import */ var _objDisplay_js__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./objDisplay.js */ "./node_modules/chai/lib/chai/utils/objDisplay.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _transferFlags_js__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(/*! ./transferFlags.js */ "./node_modules/chai/lib/chai/utils/transferFlags.js");
/* harmony import */ var deep_eql__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(/*! deep-eql */ "./node_modules/deep-eql/index.js");
/* harmony import */ var pathval__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(/*! pathval */ "./node_modules/pathval/index.js");
/* harmony import */ var _addProperty_js__WEBPACK_IMPORTED_MODULE_12__ = __webpack_require__(/*! ./addProperty.js */ "./node_modules/chai/lib/chai/utils/addProperty.js");
/* harmony import */ var _addMethod_js__WEBPACK_IMPORTED_MODULE_13__ = __webpack_require__(/*! ./addMethod.js */ "./node_modules/chai/lib/chai/utils/addMethod.js");
/* harmony import */ var _overwriteProperty_js__WEBPACK_IMPORTED_MODULE_14__ = __webpack_require__(/*! ./overwriteProperty.js */ "./node_modules/chai/lib/chai/utils/overwriteProperty.js");
/* harmony import */ var _overwriteMethod_js__WEBPACK_IMPORTED_MODULE_15__ = __webpack_require__(/*! ./overwriteMethod.js */ "./node_modules/chai/lib/chai/utils/overwriteMethod.js");
/* harmony import */ var _addChainableMethod_js__WEBPACK_IMPORTED_MODULE_16__ = __webpack_require__(/*! ./addChainableMethod.js */ "./node_modules/chai/lib/chai/utils/addChainableMethod.js");
/* harmony import */ var _overwriteChainableMethod_js__WEBPACK_IMPORTED_MODULE_17__ = __webpack_require__(/*! ./overwriteChainableMethod.js */ "./node_modules/chai/lib/chai/utils/overwriteChainableMethod.js");
/* harmony import */ var _compareByInspect_js__WEBPACK_IMPORTED_MODULE_18__ = __webpack_require__(/*! ./compareByInspect.js */ "./node_modules/chai/lib/chai/utils/compareByInspect.js");
/* harmony import */ var _getOwnEnumerablePropertySymbols_js__WEBPACK_IMPORTED_MODULE_19__ = __webpack_require__(/*! ./getOwnEnumerablePropertySymbols.js */ "./node_modules/chai/lib/chai/utils/getOwnEnumerablePropertySymbols.js");
/* harmony import */ var _getOwnEnumerableProperties_js__WEBPACK_IMPORTED_MODULE_20__ = __webpack_require__(/*! ./getOwnEnumerableProperties.js */ "./node_modules/chai/lib/chai/utils/getOwnEnumerableProperties.js");
/* harmony import */ var _proxify_js__WEBPACK_IMPORTED_MODULE_21__ = __webpack_require__(/*! ./proxify.js */ "./node_modules/chai/lib/chai/utils/proxify.js");
/* harmony import */ var _addLengthGuard_js__WEBPACK_IMPORTED_MODULE_22__ = __webpack_require__(/*! ./addLengthGuard.js */ "./node_modules/chai/lib/chai/utils/addLengthGuard.js");
/* harmony import */ var _isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_23__ = __webpack_require__(/*! ./isProxyEnabled.js */ "./node_modules/chai/lib/chai/utils/isProxyEnabled.js");
/* harmony import */ var _isNaN_js__WEBPACK_IMPORTED_MODULE_24__ = __webpack_require__(/*! ./isNaN.js */ "./node_modules/chai/lib/chai/utils/isNaN.js");
/* harmony import */ var _getOperator_js__WEBPACK_IMPORTED_MODULE_25__ = __webpack_require__(/*! ./getOperator.js */ "./node_modules/chai/lib/chai/utils/getOperator.js");
/*!
 * chai
 * Copyright(c) 2011 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

// Dependencies that are used for multiple exports are required here only once


// test utility


// type utility



// expectTypes utility


// message utility


// actual utility


// Inspect util


// Object Display util


// Flag utility


// Flag transferring utility


// Deep equal utility


// Deep path info


/**
 * Function name
 *
 * @param {Function} fn
 * @returns {string}
 */
function getName(fn) {
  return fn.name;
}

// add Property


// add Method


// overwrite Property


// overwrite Method


// Add a chainable method


// Overwrite chainable method


// Compare by inspect method


// Get own enumerable property symbols method


// Get own enumerable properties method


// Checks error against a given set of criteria


// Proxify util


// addLengthGuard util


// isProxyEnabled helper


// isNaN method


// getOperator method


/**
 * Determines if an object is a `RegExp`
 * This is used since `instanceof` will not work in virtual contexts
 *
 * @param {*} obj Object to test
 * @returns {boolean}
 */
function isRegExp(obj) {
  return Object.prototype.toString.call(obj) === '[object RegExp]';
}

/**
 * Determines if an object is numeric or not
 *
 * @param {unknown} obj Object to test
 * @returns {boolean}
 */
function isNumeric(obj) {
  return ['Number', 'BigInt'].includes((0,_type_detect_js__WEBPACK_IMPORTED_MODULE_2__.type)(obj));
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/inspect.js":
/*!*****************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/inspect.js ***!
  \*****************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   inspect: () => (/* binding */ inspect)
/* harmony export */ });
/* harmony import */ var loupe__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! loupe */ "./node_modules/loupe/lib/index.js");
/* harmony import */ var _config_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../config.js */ "./node_modules/chai/lib/chai/config.js");
// This is (almost) directly from Node.js utils
// https://github.com/joyent/node/blob/f8c335d0caf47f16d31413f89aa28eda3878e3aa/lib/util.js




/**
 * ### .inspect(obj, [showHidden], [depth], [colors])
 *
 * Echoes the value of a value. Tries to print the value out
 * in the best way possible given the different types.
 *
 * @param {object} obj The object to print out.
 * @param {boolean} showHidden Flag that shows hidden (not enumerable)
 *    properties of objects. Default is false.
 * @param {number} depth Depth in which to descend in object. Default is 2.
 * @param {boolean} colors Flag to turn on ANSI escape codes to color the
 *    output. Default is false (no coloring).
 * @returns {string}
 * @namespace Utils
 * @name inspect
 */
function inspect(obj, showHidden, depth, colors) {
  let options = {
    colors: colors,
    depth: typeof depth === 'undefined' ? 2 : depth,
    showHidden: showHidden,
    truncate: _config_js__WEBPACK_IMPORTED_MODULE_1__.config.truncateThreshold ? _config_js__WEBPACK_IMPORTED_MODULE_1__.config.truncateThreshold : Infinity
  };
  return (0,loupe__WEBPACK_IMPORTED_MODULE_0__.inspect)(obj, options);
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/isNaN.js":
/*!***************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/isNaN.js ***!
  \***************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   isNaN: () => (/* binding */ isNaN)
/* harmony export */ });
/*!
 * Chai - isNaN utility
 * Copyright(c) 2012-2015 Sakthipriyan Vairamani <thechargingvolcano@gmail.com>
 * MIT Licensed
 */

const isNaN = Number.isNaN;


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/isProxyEnabled.js":
/*!************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/isProxyEnabled.js ***!
  \************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   isProxyEnabled: () => (/* binding */ isProxyEnabled)
/* harmony export */ });
/* harmony import */ var _config_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../config.js */ "./node_modules/chai/lib/chai/config.js");


/*!
 * Chai - isProxyEnabled helper
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/**
 * ### .isProxyEnabled()
 *
 * Helper function to check if Chai's proxy protection feature is enabled. If
 * proxies are unsupported or disabled via the user's Chai config, then return
 * false. Otherwise, return true.
 *
 * @namespace Utils
 * @name isProxyEnabled
 * @returns {boolean}
 */
function isProxyEnabled() {
  return (
    _config_js__WEBPACK_IMPORTED_MODULE_0__.config.useProxy &&
    typeof Proxy !== 'undefined' &&
    typeof Reflect !== 'undefined'
  );
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/objDisplay.js":
/*!********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/objDisplay.js ***!
  \********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   objDisplay: () => (/* binding */ objDisplay)
/* harmony export */ });
/* harmony import */ var _inspect_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./inspect.js */ "./node_modules/chai/lib/chai/utils/inspect.js");
/* harmony import */ var _config_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../config.js */ "./node_modules/chai/lib/chai/config.js");
/*!
 * Chai - flag utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */




/**
 * ### .objDisplay(object)
 *
 * Determines if an object or an array matches
 * criteria to be inspected in-line for error
 * messages or should be truncated.
 *
 * @param {unknown} obj javascript object to inspect
 * @returns {string} stringified object
 * @name objDisplay
 * @namespace Utils
 * @public
 */
function objDisplay(obj) {
  let str = (0,_inspect_js__WEBPACK_IMPORTED_MODULE_0__.inspect)(obj),
    type = Object.prototype.toString.call(obj);

  if (_config_js__WEBPACK_IMPORTED_MODULE_1__.config.truncateThreshold && str.length >= _config_js__WEBPACK_IMPORTED_MODULE_1__.config.truncateThreshold) {
    if (type === '[object Function]') {
      return !obj.name || obj.name === ''
        ? '[Function]'
        : '[Function: ' + obj.name + ']';
    } else if (type === '[object Array]') {
      return '[ Array(' + obj.length + ') ]';
    } else if (type === '[object Object]') {
      let keys = Object.keys(obj),
        kstr =
          keys.length > 2
            ? keys.splice(0, 2).join(', ') + ', ...'
            : keys.join(', ');
      return '{ Object (' + kstr + ') }';
    } else {
      return str;
    }
  } else {
    return str;
  }
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/overwriteChainableMethod.js":
/*!**********************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/overwriteChainableMethod.js ***!
  \**********************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   overwriteChainableMethod: () => (/* binding */ overwriteChainableMethod)
/* harmony export */ });
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var _transferFlags_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./transferFlags.js */ "./node_modules/chai/lib/chai/utils/transferFlags.js");
/*!
 * Chai - overwriteChainableMethod utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */




/**
 * ### .overwriteChainableMethod(ctx, name, method, chainingBehavior)
 *
 * Overwrites an already existing chainable method
 * and provides access to the previous function or
 * property.  Must return functions to be used for
 * name.
 *
 *     utils.overwriteChainableMethod(chai.Assertion.prototype, 'lengthOf',
 *         function (_super) {
 *         }
 *         , function (_super) {
 *         }
 *     );
 *
 * Can also be accessed directly from `chai.Assertion`.
 *
 *     chai.Assertion.overwriteChainableMethod('foo', fn, fn);
 *
 * Then can be used as any other assertion.
 *
 *     expect(myFoo).to.have.lengthOf(3);
 *     expect(myFoo).to.have.lengthOf.above(3);
 *
 * @param {object} ctx object whose method / property is to be overwritten
 * @param {string} name of method / property to overwrite
 * @param {Function} method function that returns a function to be used for name
 * @param {Function} chainingBehavior function that returns a function to be used for property
 * @namespace Utils
 * @name overwriteChainableMethod
 * @public
 */
function overwriteChainableMethod(ctx, name, method, chainingBehavior) {
  let chainableBehavior = ctx.__methods[name];

  let _chainingBehavior = chainableBehavior.chainingBehavior;
  chainableBehavior.chainingBehavior =
    function overwritingChainableMethodGetter() {
      let result = chainingBehavior(_chainingBehavior).call(this);
      if (result !== undefined) {
        return result;
      }

      let newAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion();
      (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_1__.transferFlags)(this, newAssertion);
      return newAssertion;
    };

  let _method = chainableBehavior.method;
  chainableBehavior.method = function overwritingChainableMethodWrapper() {
    let result = method(_method).apply(this, arguments);
    if (result !== undefined) {
      return result;
    }

    let newAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion();
    (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_1__.transferFlags)(this, newAssertion);
    return newAssertion;
  };
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/overwriteMethod.js":
/*!*************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/overwriteMethod.js ***!
  \*************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   overwriteMethod: () => (/* binding */ overwriteMethod)
/* harmony export */ });
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var _addLengthGuard_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./addLengthGuard.js */ "./node_modules/chai/lib/chai/utils/addLengthGuard.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _proxify_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./proxify.js */ "./node_modules/chai/lib/chai/utils/proxify.js");
/* harmony import */ var _transferFlags_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./transferFlags.js */ "./node_modules/chai/lib/chai/utils/transferFlags.js");
/*!
 * Chai - overwriteMethod utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */







/**
 * ### .overwriteMethod(ctx, name, fn)
 *
 * Overwrites an already existing method and provides
 * access to previous function. Must return function
 * to be used for name.
 *
 *     utils.overwriteMethod(chai.Assertion.prototype, 'equal', function (_super) {
 *         return function (str) {
 *             var obj = utils.flag(this, 'object');
 *             if (obj instanceof Foo) {
 *                 new chai.Assertion(obj.value).to.equal(str);
 *             } else {
 *                 _super.apply(this, arguments);
 *             }
 *         }
 *     });
 *
 * Can also be accessed directly from `chai.Assertion`.
 *
 *     chai.Assertion.overwriteMethod('foo', fn);
 *
 * Then can be used as any other assertion.
 *
 *     expect(myFoo).to.equal('bar');
 *
 * @param {object} ctx object whose method is to be overwritten
 * @param {string} name of method to overwrite
 * @param {Function} method function that returns a function to be used for name
 * @namespace Utils
 * @name overwriteMethod
 * @public
 */
function overwriteMethod(ctx, name, method) {
  let _method = ctx[name],
    _super = function () {
      throw new Error(name + ' is not a function');
    };

  if (_method && 'function' === typeof _method) _super = _method;

  let overwritingMethodWrapper = function () {
    // Setting the `ssfi` flag to `overwritingMethodWrapper` causes this
    // function to be the starting point for removing implementation frames from
    // the stack trace of a failed assertion.
    //
    // However, we only want to use this function as the starting point if the
    // `lockSsfi` flag isn't set.
    //
    // If the `lockSsfi` flag is set, then either this assertion has been
    // overwritten by another assertion, or this assertion is being invoked from
    // inside of another assertion. In the first case, the `ssfi` flag has
    // already been set by the overwriting assertion. In the second case, the
    // `ssfi` flag has already been set by the outer assertion.
    if (!(0,_flag_js__WEBPACK_IMPORTED_MODULE_2__.flag)(this, 'lockSsfi')) {
      (0,_flag_js__WEBPACK_IMPORTED_MODULE_2__.flag)(this, 'ssfi', overwritingMethodWrapper);
    }

    // Setting the `lockSsfi` flag to `true` prevents the overwritten assertion
    // from changing the `ssfi` flag. By this point, the `ssfi` flag is already
    // set to the correct starting point for this assertion.
    let origLockSsfi = (0,_flag_js__WEBPACK_IMPORTED_MODULE_2__.flag)(this, 'lockSsfi');
    (0,_flag_js__WEBPACK_IMPORTED_MODULE_2__.flag)(this, 'lockSsfi', true);
    let result = method(_super).apply(this, arguments);
    (0,_flag_js__WEBPACK_IMPORTED_MODULE_2__.flag)(this, 'lockSsfi', origLockSsfi);

    if (result !== undefined) {
      return result;
    }

    let newAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion();
    (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_4__.transferFlags)(this, newAssertion);
    return newAssertion;
  };

  (0,_addLengthGuard_js__WEBPACK_IMPORTED_MODULE_1__.addLengthGuard)(overwritingMethodWrapper, name, false);
  ctx[name] = (0,_proxify_js__WEBPACK_IMPORTED_MODULE_3__.proxify)(overwritingMethodWrapper, name);
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/overwriteProperty.js":
/*!***************************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/overwriteProperty.js ***!
  \***************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   overwriteProperty: () => (/* binding */ overwriteProperty)
/* harmony export */ });
/* harmony import */ var _assertion_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../assertion.js */ "./node_modules/chai/lib/chai/assertion.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./isProxyEnabled.js */ "./node_modules/chai/lib/chai/utils/isProxyEnabled.js");
/* harmony import */ var _transferFlags_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./transferFlags.js */ "./node_modules/chai/lib/chai/utils/transferFlags.js");
/*!
 * Chai - overwriteProperty utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */






/**
 * ### .overwriteProperty(ctx, name, fn)
 *
 * Overwrites an already existing property getter and provides
 * access to previous value. Must return function to use as getter.
 *
 *     utils.overwriteProperty(chai.Assertion.prototype, 'ok', function (_super) {
 *         return function () {
 *             var obj = utils.flag(this, 'object');
 *             if (obj instanceof Foo) {
 *                 new chai.Assertion(obj.name).to.equal('bar');
 *             } else {
 *                 _super.call(this);
 *             }
 *         }
 *     });
 *
 * Can also be accessed directly from `chai.Assertion`.
 *
 *     chai.Assertion.overwriteProperty('foo', fn);
 *
 * Then can be used as any other assertion.
 *
 *     expect(myFoo).to.be.ok;
 *
 * @param {object} ctx object whose property is to be overwritten
 * @param {string} name of property to overwrite
 * @param {Function} getter function that returns a getter function to be used for name
 * @namespace Utils
 * @name overwriteProperty
 * @public
 */
function overwriteProperty(ctx, name, getter) {
  let _get = Object.getOwnPropertyDescriptor(ctx, name),
    _super = function () {};

  if (_get && 'function' === typeof _get.get) _super = _get.get;

  Object.defineProperty(ctx, name, {
    get: function overwritingPropertyGetter() {
      // Setting the `ssfi` flag to `overwritingPropertyGetter` causes this
      // function to be the starting point for removing implementation frames
      // from the stack trace of a failed assertion.
      //
      // However, we only want to use this function as the starting point if
      // the `lockSsfi` flag isn't set and proxy protection is disabled.
      //
      // If the `lockSsfi` flag is set, then either this assertion has been
      // overwritten by another assertion, or this assertion is being invoked
      // from inside of another assertion. In the first case, the `ssfi` flag
      // has already been set by the overwriting assertion. In the second
      // case, the `ssfi` flag has already been set by the outer assertion.
      //
      // If proxy protection is enabled, then the `ssfi` flag has already been
      // set by the proxy getter.
      if (!(0,_isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_2__.isProxyEnabled)() && !(0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'lockSsfi')) {
        (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'ssfi', overwritingPropertyGetter);
      }

      // Setting the `lockSsfi` flag to `true` prevents the overwritten
      // assertion from changing the `ssfi` flag. By this point, the `ssfi`
      // flag is already set to the correct starting point for this assertion.
      let origLockSsfi = (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'lockSsfi');
      (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'lockSsfi', true);
      let result = getter(_super).call(this);
      (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(this, 'lockSsfi', origLockSsfi);

      if (result !== undefined) {
        return result;
      }

      let newAssertion = new _assertion_js__WEBPACK_IMPORTED_MODULE_0__.Assertion();
      (0,_transferFlags_js__WEBPACK_IMPORTED_MODULE_3__.transferFlags)(this, newAssertion);
      return newAssertion;
    },
    configurable: true
  });
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/proxify.js":
/*!*****************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/proxify.js ***!
  \*****************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   proxify: () => (/* binding */ proxify)
/* harmony export */ });
/* harmony import */ var _config_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../config.js */ "./node_modules/chai/lib/chai/config.js");
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/* harmony import */ var _getProperties_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./getProperties.js */ "./node_modules/chai/lib/chai/utils/getProperties.js");
/* harmony import */ var _isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./isProxyEnabled.js */ "./node_modules/chai/lib/chai/utils/isProxyEnabled.js");





/*!
 * Chai - proxify utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/** @type {PropertyKey[]} */
const builtins = ['__flags', '__methods', '_obj', 'assert'];

/**
 * ### .proxify(object)
 *
 * Return a proxy of given object that throws an error when a non-existent
 * property is read. By default, the root cause is assumed to be a misspelled
 * property, and thus an attempt is made to offer a reasonable suggestion from
 * the list of existing properties. However, if a nonChainableMethodName is
 * provided, then the root cause is instead a failure to invoke a non-chainable
 * method prior to reading the non-existent property.
 *
 * If proxies are unsupported or disabled via the user's Chai config, then
 * return object without modification.
 *
 * @namespace Utils
 * @template {object} T
 * @param {T} obj
 * @param {string} [nonChainableMethodName]
 * @returns {T}
 */
function proxify(obj, nonChainableMethodName) {
  if (!(0,_isProxyEnabled_js__WEBPACK_IMPORTED_MODULE_3__.isProxyEnabled)()) return obj;

  return new Proxy(obj, {
    get: function proxyGetter(target, property) {
      // This check is here because we should not throw errors on Symbol properties
      // such as `Symbol.toStringTag`.
      // The values for which an error should be thrown can be configured using
      // the `config.proxyExcludedKeys` setting.
      if (
        typeof property === 'string' &&
        _config_js__WEBPACK_IMPORTED_MODULE_0__.config.proxyExcludedKeys.indexOf(property) === -1 &&
        !Reflect.has(target, property)
      ) {
        // Special message for invalid property access of non-chainable methods.
        if (nonChainableMethodName) {
          throw Error(
            'Invalid Chai property: ' +
              nonChainableMethodName +
              '.' +
              property +
              '. See docs for proper usage of "' +
              nonChainableMethodName +
              '".'
          );
        }

        // If the property is reasonably close to an existing Chai property,
        // suggest that property to the user. Only suggest properties with a
        // distance less than 4.
        let suggestion = null;
        let suggestionDistance = 4;
        (0,_getProperties_js__WEBPACK_IMPORTED_MODULE_2__.getProperties)(target).forEach(function (prop) {
          if (
            // we actually mean to check `Object.prototype` here
            // eslint-disable-next-line no-prototype-builtins
            !Object.prototype.hasOwnProperty(prop) &&
            builtins.indexOf(prop) === -1
          ) {
            let dist = stringDistanceCapped(property, prop, suggestionDistance);
            if (dist < suggestionDistance) {
              suggestion = prop;
              suggestionDistance = dist;
            }
          }
        });

        if (suggestion !== null) {
          throw Error(
            'Invalid Chai property: ' +
              property +
              '. Did you mean "' +
              suggestion +
              '"?'
          );
        } else {
          throw Error('Invalid Chai property: ' + property);
        }
      }

      // Use this proxy getter as the starting point for removing implementation
      // frames from the stack trace of a failed assertion. For property
      // assertions, this prevents the proxy getter from showing up in the stack
      // trace since it's invoked before the property getter. For method and
      // chainable method assertions, this flag will end up getting changed to
      // the method wrapper, which is good since this frame will no longer be in
      // the stack once the method is invoked. Note that Chai builtin assertion
      // properties such as `__flags` are skipped since this is only meant to
      // capture the starting point of an assertion. This step is also skipped
      // if the `lockSsfi` flag is set, thus indicating that this assertion is
      // being called from within another assertion. In that case, the `ssfi`
      // flag is already set to the outer assertion's starting point.
      if (builtins.indexOf(property) === -1 && !(0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(target, 'lockSsfi')) {
        (0,_flag_js__WEBPACK_IMPORTED_MODULE_1__.flag)(target, 'ssfi', proxyGetter);
      }

      return Reflect.get(target, property);
    }
  });
}

/**
 * # stringDistanceCapped(strA, strB, cap)
 * Return the Levenshtein distance between two strings, but no more than cap.
 *
 * @param {string} strA
 * @param {string} strB
 * @param {number} cap
 * @returns {number} min(string distance between strA and strB, cap)
 * @private
 */
function stringDistanceCapped(strA, strB, cap) {
  if (Math.abs(strA.length - strB.length) >= cap) {
    return cap;
  }

  let memo = [];
  // `memo` is a two-dimensional array containing distances.
  // memo[i][j] is the distance between strA.slice(0, i) and
  // strB.slice(0, j).
  for (let i = 0; i <= strA.length; i++) {
    memo[i] = Array(strB.length + 1).fill(0);
    memo[i][0] = i;
  }
  for (let j = 0; j < strB.length; j++) {
    memo[0][j] = j;
  }

  for (let i = 1; i <= strA.length; i++) {
    let ch = strA.charCodeAt(i - 1);
    for (let j = 1; j <= strB.length; j++) {
      if (Math.abs(i - j) >= cap) {
        memo[i][j] = cap;
        continue;
      }
      memo[i][j] = Math.min(
        memo[i - 1][j] + 1,
        memo[i][j - 1] + 1,
        memo[i - 1][j - 1] + (ch === strB.charCodeAt(j - 1) ? 0 : 1)
      );
    }
  }

  return memo[strA.length][strB.length];
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/test.js":
/*!**************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/test.js ***!
  \**************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   test: () => (/* binding */ test)
/* harmony export */ });
/* harmony import */ var _flag_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./flag.js */ "./node_modules/chai/lib/chai/utils/flag.js");
/*!
 * Chai - test utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */



/**
 * ### .test(object, expression)
 *
 * Test an object for expression.
 *
 * @param {object} obj (constructed Assertion)
 * @param {unknown} args
 * @returns {unknown}
 * @namespace Utils
 * @name test
 */
function test(obj, args) {
  let negate = (0,_flag_js__WEBPACK_IMPORTED_MODULE_0__.flag)(obj, 'negate'),
    expr = args[0];
  return negate ? !expr : expr;
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/transferFlags.js":
/*!***********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/transferFlags.js ***!
  \***********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   transferFlags: () => (/* binding */ transferFlags)
/* harmony export */ });
/*!
 * Chai - transferFlags utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

/**
 * ### .transferFlags(assertion, object, includeAll = true)
 *
 * Transfer all the flags for `assertion` to `object`. If
 * `includeAll` is set to `false`, then the base Chai
 * assertion flags (namely `object`, `ssfi`, `lockSsfi`,
 * and `message`) will not be transferred.
 *
 *     var newAssertion = new Assertion();
 *     utils.transferFlags(assertion, newAssertion);
 *
 *     var anotherAssertion = new Assertion(myObj);
 *     utils.transferFlags(assertion, anotherAssertion, false);
 *
 * @param {import('../assertion.js').Assertion} assertion the assertion to transfer the flags from
 * @param {object} object the object to transfer the flags to; usually a new assertion
 * @param {boolean} includeAll
 * @namespace Utils
 * @name transferFlags
 * @private
 */
function transferFlags(assertion, object, includeAll) {
  let flags = assertion.__flags || (assertion.__flags = Object.create(null));

  if (!object.__flags) {
    object.__flags = Object.create(null);
  }

  includeAll = arguments.length === 3 ? includeAll : true;

  for (let flag in flags) {
    if (
      includeAll ||
      (flag !== 'object' &&
        flag !== 'ssfi' &&
        flag !== 'lockSsfi' &&
        flag != 'message')
    ) {
      object.__flags[flag] = flags[flag];
    }
  }
}


/***/ }),

/***/ "./node_modules/chai/lib/chai/utils/type-detect.js":
/*!*********************************************************!*\
  !*** ./node_modules/chai/lib/chai/utils/type-detect.js ***!
  \*********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   type: () => (/* binding */ type)
/* harmony export */ });
/**
 * @param {unknown} obj
 * @returns {string}
 */
function type(obj) {
  if (typeof obj === 'undefined') {
    return 'undefined';
  }

  if (obj === null) {
    return 'null';
  }

  const stringTag = obj[Symbol.toStringTag];
  if (typeof stringTag === 'string') {
    return stringTag;
  }
  const type = Object.prototype.toString.call(obj).slice(8, -1);
  return type;
}


/***/ }),

/***/ "./node_modules/check-error/index.js":
/*!*******************************************!*\
  !*** ./node_modules/check-error/index.js ***!
  \*******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   compatibleConstructor: () => (/* binding */ compatibleConstructor),
/* harmony export */   compatibleInstance: () => (/* binding */ compatibleInstance),
/* harmony export */   compatibleMessage: () => (/* binding */ compatibleMessage),
/* harmony export */   getConstructorName: () => (/* binding */ getConstructorName),
/* harmony export */   getMessage: () => (/* binding */ getMessage)
/* harmony export */ });
function isErrorInstance(obj) {
  // eslint-disable-next-line prefer-reflect
  return obj instanceof Error || Object.prototype.toString.call(obj) === '[object Error]';
}

function isRegExp(obj) {
  // eslint-disable-next-line prefer-reflect
  return Object.prototype.toString.call(obj) === '[object RegExp]';
}

/**
 * ### .compatibleInstance(thrown, errorLike)
 *
 * Checks if two instances are compatible (strict equal).
 * Returns false if errorLike is not an instance of Error, because instances
 * can only be compatible if they're both error instances.
 *
 * @name compatibleInstance
 * @param {Error} thrown error
 * @param {Error|ErrorConstructor} errorLike object to compare against
 * @namespace Utils
 * @api public
 */

function compatibleInstance(thrown, errorLike) {
  return isErrorInstance(errorLike) && thrown === errorLike;
}

/**
 * ### .compatibleConstructor(thrown, errorLike)
 *
 * Checks if two constructors are compatible.
 * This function can receive either an error constructor or
 * an error instance as the `errorLike` argument.
 * Constructors are compatible if they're the same or if one is
 * an instance of another.
 *
 * @name compatibleConstructor
 * @param {Error} thrown error
 * @param {Error|ErrorConstructor} errorLike object to compare against
 * @namespace Utils
 * @api public
 */

function compatibleConstructor(thrown, errorLike) {
  if (isErrorInstance(errorLike)) {
    // If `errorLike` is an instance of any error we compare their constructors
    return thrown.constructor === errorLike.constructor || thrown instanceof errorLike.constructor;
  } else if ((typeof errorLike === 'object' || typeof errorLike === 'function') && errorLike.prototype) {
    // If `errorLike` is a constructor that inherits from Error, we compare `thrown` to `errorLike` directly
    return thrown.constructor === errorLike || thrown instanceof errorLike;
  }

  return false;
}

/**
 * ### .compatibleMessage(thrown, errMatcher)
 *
 * Checks if an error's message is compatible with a matcher (String or RegExp).
 * If the message contains the String or passes the RegExp test,
 * it is considered compatible.
 *
 * @name compatibleMessage
 * @param {Error} thrown error
 * @param {String|RegExp} errMatcher to look for into the message
 * @namespace Utils
 * @api public
 */

function compatibleMessage(thrown, errMatcher) {
  const comparisonString = typeof thrown === 'string' ? thrown : thrown.message;
  if (isRegExp(errMatcher)) {
    return errMatcher.test(comparisonString);
  } else if (typeof errMatcher === 'string') {
    return comparisonString.indexOf(errMatcher) !== -1; // eslint-disable-line no-magic-numbers
  }

  return false;
}

/**
 * ### .getConstructorName(errorLike)
 *
 * Gets the constructor name for an Error instance or constructor itself.
 *
 * @name getConstructorName
 * @param {Error|ErrorConstructor} errorLike
 * @namespace Utils
 * @api public
 */

function getConstructorName(errorLike) {
  let constructorName = errorLike;
  if (isErrorInstance(errorLike)) {
    constructorName = errorLike.constructor.name;
  } else if (typeof errorLike === 'function') {
    // If `err` is not an instance of Error it is an error constructor itself or another function.
    // If we've got a common function we get its name, otherwise we may need to create a new instance
    // of the error just in case it's a poorly-constructed error. Please see chaijs/chai/issues/45 to know more.
    constructorName = errorLike.name;
    if (constructorName === '') {
      const newConstructorName = (new errorLike().name); // eslint-disable-line new-cap
      constructorName = newConstructorName || constructorName;
    }
  }

  return constructorName;
}

/**
 * ### .getMessage(errorLike)
 *
 * Gets the error message from an error.
 * If `err` is a String itself, we return it.
 * If the error has no message, we return an empty string.
 *
 * @name getMessage
 * @param {Error|String} errorLike
 * @namespace Utils
 * @api public
 */

function getMessage(errorLike) {
  let msg = '';
  if (errorLike && errorLike.message) {
    msg = errorLike.message;
  } else if (typeof errorLike === 'string') {
    msg = errorLike;
  }

  return msg;
}




/***/ }),

/***/ "./node_modules/deep-eql/index.js":
/*!****************************************!*\
  !*** ./node_modules/deep-eql/index.js ***!
  \****************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   MemoizeMap: () => (/* binding */ MemoizeMap),
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* globals Symbol: false, Uint8Array: false, WeakMap: false */
/*!
 * deep-eql
 * Copyright(c) 2013 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */

function type(obj) {
  if (typeof obj === 'undefined') {
    return 'undefined';
  }

  if (obj === null) {
    return 'null';
  }

  const stringTag = obj[Symbol.toStringTag];
  if (typeof stringTag === 'string') {
    return stringTag;
  }
  const sliceStart = 8;
  const sliceEnd = -1;
  return Object.prototype.toString.call(obj).slice(sliceStart, sliceEnd);
}

function FakeMap() {
  this._key = 'chai/deep-eql__' + Math.random() + Date.now();
}

FakeMap.prototype = {
  get: function get(key) {
    return key[this._key];
  },
  set: function set(key, value) {
    if (Object.isExtensible(key)) {
      Object.defineProperty(key, this._key, {
        value: value,
        configurable: true,
      });
    }
  },
};

var MemoizeMap = typeof WeakMap === 'function' ? WeakMap : FakeMap;
/*!
 * Check to see if the MemoizeMap has recorded a result of the two operands
 *
 * @param {Mixed} leftHandOperand
 * @param {Mixed} rightHandOperand
 * @param {MemoizeMap} memoizeMap
 * @returns {Boolean|null} result
*/
function memoizeCompare(leftHandOperand, rightHandOperand, memoizeMap) {
  // Technically, WeakMap keys can *only* be objects, not primitives.
  if (!memoizeMap || isPrimitive(leftHandOperand) || isPrimitive(rightHandOperand)) {
    return null;
  }
  var leftHandMap = memoizeMap.get(leftHandOperand);
  if (leftHandMap) {
    var result = leftHandMap.get(rightHandOperand);
    if (typeof result === 'boolean') {
      return result;
    }
  }
  return null;
}

/*!
 * Set the result of the equality into the MemoizeMap
 *
 * @param {Mixed} leftHandOperand
 * @param {Mixed} rightHandOperand
 * @param {MemoizeMap} memoizeMap
 * @param {Boolean} result
*/
function memoizeSet(leftHandOperand, rightHandOperand, memoizeMap, result) {
  // Technically, WeakMap keys can *only* be objects, not primitives.
  if (!memoizeMap || isPrimitive(leftHandOperand) || isPrimitive(rightHandOperand)) {
    return;
  }
  var leftHandMap = memoizeMap.get(leftHandOperand);
  if (leftHandMap) {
    leftHandMap.set(rightHandOperand, result);
  } else {
    leftHandMap = new MemoizeMap();
    leftHandMap.set(rightHandOperand, result);
    memoizeMap.set(leftHandOperand, leftHandMap);
  }
}

/*!
 * Primary Export
 */

/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (deepEqual);

/**
 * Assert deeply nested sameValue equality between two objects of any type.
 *
 * @param {Mixed} leftHandOperand
 * @param {Mixed} rightHandOperand
 * @param {Object} [options] (optional) Additional options
 * @param {Array} [options.comparator] (optional) Override default algorithm, determining custom equality.
 * @param {Array} [options.memoize] (optional) Provide a custom memoization object which will cache the results of
    complex objects for a speed boost. By passing `false` you can disable memoization, but this will cause circular
    references to blow the stack.
 * @return {Boolean} equal match
 */
function deepEqual(leftHandOperand, rightHandOperand, options) {
  // If we have a comparator, we can't assume anything; so bail to its check first.
  if (options && options.comparator) {
    return extensiveDeepEqual(leftHandOperand, rightHandOperand, options);
  }

  var simpleResult = simpleEqual(leftHandOperand, rightHandOperand);
  if (simpleResult !== null) {
    return simpleResult;
  }

  // Deeper comparisons are pushed through to a larger function
  return extensiveDeepEqual(leftHandOperand, rightHandOperand, options);
}

/**
 * Many comparisons can be canceled out early via simple equality or primitive checks.
 * @param {Mixed} leftHandOperand
 * @param {Mixed} rightHandOperand
 * @return {Boolean|null} equal match
 */
function simpleEqual(leftHandOperand, rightHandOperand) {
  // Equal references (except for Numbers) can be returned early
  if (leftHandOperand === rightHandOperand) {
    // Handle +-0 cases
    return leftHandOperand !== 0 || 1 / leftHandOperand === 1 / rightHandOperand;
  }

  // handle NaN cases
  if (
    leftHandOperand !== leftHandOperand && // eslint-disable-line no-self-compare
    rightHandOperand !== rightHandOperand // eslint-disable-line no-self-compare
  ) {
    return true;
  }

  // Anything that is not an 'object', i.e. symbols, functions, booleans, numbers,
  // strings, and undefined, can be compared by reference.
  if (isPrimitive(leftHandOperand) || isPrimitive(rightHandOperand)) {
    // Easy out b/c it would have passed the first equality check
    return false;
  }
  return null;
}

/*!
 * The main logic of the `deepEqual` function.
 *
 * @param {Mixed} leftHandOperand
 * @param {Mixed} rightHandOperand
 * @param {Object} [options] (optional) Additional options
 * @param {Array} [options.comparator] (optional) Override default algorithm, determining custom equality.
 * @param {Array} [options.memoize] (optional) Provide a custom memoization object which will cache the results of
    complex objects for a speed boost. By passing `false` you can disable memoization, but this will cause circular
    references to blow the stack.
 * @return {Boolean} equal match
*/
function extensiveDeepEqual(leftHandOperand, rightHandOperand, options) {
  options = options || {};
  options.memoize = options.memoize === false ? false : options.memoize || new MemoizeMap();
  var comparator = options && options.comparator;

  // Check if a memoized result exists.
  var memoizeResultLeft = memoizeCompare(leftHandOperand, rightHandOperand, options.memoize);
  if (memoizeResultLeft !== null) {
    return memoizeResultLeft;
  }
  var memoizeResultRight = memoizeCompare(rightHandOperand, leftHandOperand, options.memoize);
  if (memoizeResultRight !== null) {
    return memoizeResultRight;
  }

  // If a comparator is present, use it.
  if (comparator) {
    var comparatorResult = comparator(leftHandOperand, rightHandOperand);
    // Comparators may return null, in which case we want to go back to default behavior.
    if (comparatorResult === false || comparatorResult === true) {
      memoizeSet(leftHandOperand, rightHandOperand, options.memoize, comparatorResult);
      return comparatorResult;
    }
    // To allow comparators to override *any* behavior, we ran them first. Since it didn't decide
    // what to do, we need to make sure to return the basic tests first before we move on.
    var simpleResult = simpleEqual(leftHandOperand, rightHandOperand);
    if (simpleResult !== null) {
      // Don't memoize this, it takes longer to set/retrieve than to just compare.
      return simpleResult;
    }
  }

  var leftHandType = type(leftHandOperand);
  if (leftHandType !== type(rightHandOperand)) {
    memoizeSet(leftHandOperand, rightHandOperand, options.memoize, false);
    return false;
  }

  // Temporarily set the operands in the memoize object to prevent blowing the stack
  memoizeSet(leftHandOperand, rightHandOperand, options.memoize, true);

  var result = extensiveDeepEqualByType(leftHandOperand, rightHandOperand, leftHandType, options);
  memoizeSet(leftHandOperand, rightHandOperand, options.memoize, result);
  return result;
}

function extensiveDeepEqualByType(leftHandOperand, rightHandOperand, leftHandType, options) {
  switch (leftHandType) {
    case 'String':
    case 'Number':
    case 'Boolean':
    case 'Date':
      // If these types are their instance types (e.g. `new Number`) then re-deepEqual against their values
      return deepEqual(leftHandOperand.valueOf(), rightHandOperand.valueOf());
    case 'Promise':
    case 'Symbol':
    case 'function':
    case 'WeakMap':
    case 'WeakSet':
      return leftHandOperand === rightHandOperand;
    case 'Error':
      return keysEqual(leftHandOperand, rightHandOperand, [ 'name', 'message', 'code' ], options);
    case 'Arguments':
    case 'Int8Array':
    case 'Uint8Array':
    case 'Uint8ClampedArray':
    case 'Int16Array':
    case 'Uint16Array':
    case 'Int32Array':
    case 'Uint32Array':
    case 'Float32Array':
    case 'Float64Array':
    case 'Array':
      return iterableEqual(leftHandOperand, rightHandOperand, options);
    case 'RegExp':
      return regexpEqual(leftHandOperand, rightHandOperand);
    case 'Generator':
      return generatorEqual(leftHandOperand, rightHandOperand, options);
    case 'DataView':
      return iterableEqual(new Uint8Array(leftHandOperand.buffer), new Uint8Array(rightHandOperand.buffer), options);
    case 'ArrayBuffer':
      return iterableEqual(new Uint8Array(leftHandOperand), new Uint8Array(rightHandOperand), options);
    case 'Set':
      return entriesEqual(leftHandOperand, rightHandOperand, options);
    case 'Map':
      return entriesEqual(leftHandOperand, rightHandOperand, options);
    case 'Temporal.PlainDate':
    case 'Temporal.PlainTime':
    case 'Temporal.PlainDateTime':
    case 'Temporal.Instant':
    case 'Temporal.ZonedDateTime':
    case 'Temporal.PlainYearMonth':
    case 'Temporal.PlainMonthDay':
      return leftHandOperand.equals(rightHandOperand);
    case 'Temporal.Duration':
      return leftHandOperand.total('nanoseconds') === rightHandOperand.total('nanoseconds');
    case 'Temporal.TimeZone':
    case 'Temporal.Calendar':
      return leftHandOperand.toString() === rightHandOperand.toString();
    default:
      return objectEqual(leftHandOperand, rightHandOperand, options);
  }
}

/*!
 * Compare two Regular Expressions for equality.
 *
 * @param {RegExp} leftHandOperand
 * @param {RegExp} rightHandOperand
 * @return {Boolean} result
 */

function regexpEqual(leftHandOperand, rightHandOperand) {
  return leftHandOperand.toString() === rightHandOperand.toString();
}

/*!
 * Compare two Sets/Maps for equality. Faster than other equality functions.
 *
 * @param {Set} leftHandOperand
 * @param {Set} rightHandOperand
 * @param {Object} [options] (Optional)
 * @return {Boolean} result
 */

function entriesEqual(leftHandOperand, rightHandOperand, options) {
  try {
    // IE11 doesn't support Set#entries or Set#@@iterator, so we need manually populate using Set#forEach
    if (leftHandOperand.size !== rightHandOperand.size) {
      return false;
    }
    if (leftHandOperand.size === 0) {
      return true;
    }
  } catch (sizeError) {
    // things that aren't actual Maps or Sets will throw here
    return false;
  }
  var leftHandItems = [];
  var rightHandItems = [];
  leftHandOperand.forEach(function gatherEntries(key, value) {
    leftHandItems.push([ key, value ]);
  });
  rightHandOperand.forEach(function gatherEntries(key, value) {
    rightHandItems.push([ key, value ]);
  });
  return iterableEqual(leftHandItems.sort(), rightHandItems.sort(), options);
}

/*!
 * Simple equality for flat iterable objects such as Arrays, TypedArrays or Node.js buffers.
 *
 * @param {Iterable} leftHandOperand
 * @param {Iterable} rightHandOperand
 * @param {Object} [options] (Optional)
 * @return {Boolean} result
 */

function iterableEqual(leftHandOperand, rightHandOperand, options) {
  var length = leftHandOperand.length;
  if (length !== rightHandOperand.length) {
    return false;
  }
  if (length === 0) {
    return true;
  }
  var index = -1;
  while (++index < length) {
    if (deepEqual(leftHandOperand[index], rightHandOperand[index], options) === false) {
      return false;
    }
  }
  return true;
}

/*!
 * Simple equality for generator objects such as those returned by generator functions.
 *
 * @param {Iterable} leftHandOperand
 * @param {Iterable} rightHandOperand
 * @param {Object} [options] (Optional)
 * @return {Boolean} result
 */

function generatorEqual(leftHandOperand, rightHandOperand, options) {
  return iterableEqual(getGeneratorEntries(leftHandOperand), getGeneratorEntries(rightHandOperand), options);
}

/*!
 * Determine if the given object has an @@iterator function.
 *
 * @param {Object} target
 * @return {Boolean} `true` if the object has an @@iterator function.
 */
function hasIteratorFunction(target) {
  return typeof Symbol !== 'undefined' &&
    typeof target === 'object' &&
    typeof Symbol.iterator !== 'undefined' &&
    typeof target[Symbol.iterator] === 'function';
}

/*!
 * Gets all iterator entries from the given Object. If the Object has no @@iterator function, returns an empty array.
 * This will consume the iterator - which could have side effects depending on the @@iterator implementation.
 *
 * @param {Object} target
 * @returns {Array} an array of entries from the @@iterator function
 */
function getIteratorEntries(target) {
  if (hasIteratorFunction(target)) {
    try {
      return getGeneratorEntries(target[Symbol.iterator]());
    } catch (iteratorError) {
      return [];
    }
  }
  return [];
}

/*!
 * Gets all entries from a Generator. This will consume the generator - which could have side effects.
 *
 * @param {Generator} target
 * @returns {Array} an array of entries from the Generator.
 */
function getGeneratorEntries(generator) {
  var generatorResult = generator.next();
  var accumulator = [ generatorResult.value ];
  while (generatorResult.done === false) {
    generatorResult = generator.next();
    accumulator.push(generatorResult.value);
  }
  return accumulator;
}

/*!
 * Gets all own and inherited enumerable keys from a target.
 *
 * @param {Object} target
 * @returns {Array} an array of own and inherited enumerable keys from the target.
 */
function getEnumerableKeys(target) {
  var keys = [];
  for (var key in target) {
    keys.push(key);
  }
  return keys;
}

function getEnumerableSymbols(target) {
  var keys = [];
  var allKeys = Object.getOwnPropertySymbols(target);
  for (var i = 0; i < allKeys.length; i += 1) {
    var key = allKeys[i];
    if (Object.getOwnPropertyDescriptor(target, key).enumerable) {
      keys.push(key);
    }
  }
  return keys;
}

/*!
 * Determines if two objects have matching values, given a set of keys. Defers to deepEqual for the equality check of
 * each key. If any value of the given key is not equal, the function will return false (early).
 *
 * @param {Mixed} leftHandOperand
 * @param {Mixed} rightHandOperand
 * @param {Array} keys An array of keys to compare the values of leftHandOperand and rightHandOperand against
 * @param {Object} [options] (Optional)
 * @return {Boolean} result
 */
function keysEqual(leftHandOperand, rightHandOperand, keys, options) {
  var length = keys.length;
  if (length === 0) {
    return true;
  }
  for (var i = 0; i < length; i += 1) {
    if (deepEqual(leftHandOperand[keys[i]], rightHandOperand[keys[i]], options) === false) {
      return false;
    }
  }
  return true;
}

/*!
 * Recursively check the equality of two Objects. Once basic sameness has been established it will defer to `deepEqual`
 * for each enumerable key in the object.
 *
 * @param {Mixed} leftHandOperand
 * @param {Mixed} rightHandOperand
 * @param {Object} [options] (Optional)
 * @return {Boolean} result
 */
function objectEqual(leftHandOperand, rightHandOperand, options) {
  var leftHandKeys = getEnumerableKeys(leftHandOperand);
  var rightHandKeys = getEnumerableKeys(rightHandOperand);
  var leftHandSymbols = getEnumerableSymbols(leftHandOperand);
  var rightHandSymbols = getEnumerableSymbols(rightHandOperand);
  leftHandKeys = leftHandKeys.concat(leftHandSymbols);
  rightHandKeys = rightHandKeys.concat(rightHandSymbols);

  if (leftHandKeys.length && leftHandKeys.length === rightHandKeys.length) {
    if (iterableEqual(mapSymbols(leftHandKeys).sort(), mapSymbols(rightHandKeys).sort()) === false) {
      return false;
    }
    return keysEqual(leftHandOperand, rightHandOperand, leftHandKeys, options);
  }

  var leftHandEntries = getIteratorEntries(leftHandOperand);
  var rightHandEntries = getIteratorEntries(rightHandOperand);
  if (leftHandEntries.length && leftHandEntries.length === rightHandEntries.length) {
    leftHandEntries.sort();
    rightHandEntries.sort();
    return iterableEqual(leftHandEntries, rightHandEntries, options);
  }

  if (leftHandKeys.length === 0 &&
      leftHandEntries.length === 0 &&
      rightHandKeys.length === 0 &&
      rightHandEntries.length === 0) {
    return true;
  }

  return false;
}

/*!
 * Returns true if the argument is a primitive.
 *
 * This intentionally returns true for all objects that can be compared by reference,
 * including functions and symbols.
 *
 * @param {Mixed} value
 * @return {Boolean} result
 */
function isPrimitive(value) {
  return value === null || typeof value !== 'object';
}

function mapSymbols(arr) {
  return arr.map(function mapSymbol(entry) {
    if (typeof entry === 'symbol') {
      return entry.toString();
    }

    return entry;
  });
}


/***/ }),

/***/ "./node_modules/loupe/lib/arguments.js":
/*!*********************************************!*\
  !*** ./node_modules/loupe/lib/arguments.js ***!
  \*********************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectArguments)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectArguments(args, options) {
    if (args.length === 0)
        return 'Arguments[]';
    options.truncate -= 13;
    return `Arguments[ ${(0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(args, options)} ]`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/array.js":
/*!*****************************************!*\
  !*** ./node_modules/loupe/lib/array.js ***!
  \*****************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectArray)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectArray(array, options) {
    // Object.keys will always output the Array indices first, so we can slice by
    // `array.length` to get non-index properties
    const nonIndexProperties = Object.keys(array).slice(array.length);
    if (!array.length && !nonIndexProperties.length)
        return '[]';
    options.truncate -= 4;
    const listContents = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(array, options);
    options.truncate -= listContents.length;
    let propertyContents = '';
    if (nonIndexProperties.length) {
        propertyContents = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(nonIndexProperties.map(key => [key, array[key]]), options, _helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectProperty);
    }
    return `[ ${listContents}${propertyContents ? `, ${propertyContents}` : ''} ]`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/bigint.js":
/*!******************************************!*\
  !*** ./node_modules/loupe/lib/bigint.js ***!
  \******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectBigInt)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectBigInt(number, options) {
    let nums = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(number.toString(), options.truncate - 1);
    if (nums !== _helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncator)
        nums += 'n';
    return options.stylize(nums, 'bigint');
}


/***/ }),

/***/ "./node_modules/loupe/lib/class.js":
/*!*****************************************!*\
  !*** ./node_modules/loupe/lib/class.js ***!
  \*****************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectClass)
/* harmony export */ });
/* harmony import */ var _object_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./object.js */ "./node_modules/loupe/lib/object.js");

const toStringTag = typeof Symbol !== 'undefined' && Symbol.toStringTag ? Symbol.toStringTag : false;
function inspectClass(value, options) {
    let name = '';
    if (toStringTag && toStringTag in value) {
        name = value[toStringTag];
    }
    name = name || value.constructor.name;
    // Babel transforms anonymous classes to the name `_class`
    if (!name || name === '_class') {
        name = '<Anonymous Class>';
    }
    options.truncate -= name.length;
    return `${name}${(0,_object_js__WEBPACK_IMPORTED_MODULE_0__["default"])(value, options)}`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/date.js":
/*!****************************************!*\
  !*** ./node_modules/loupe/lib/date.js ***!
  \****************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectDate)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectDate(dateObject, options) {
    const stringRepresentation = dateObject.toJSON();
    if (stringRepresentation === null) {
        return 'Invalid Date';
    }
    const split = stringRepresentation.split('T');
    const date = split[0];
    // If we need to - truncate the time portion, but never the date
    return options.stylize(`${date}T${(0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(split[1], options.truncate - date.length - 1)}`, 'date');
}


/***/ }),

/***/ "./node_modules/loupe/lib/error.js":
/*!*****************************************!*\
  !*** ./node_modules/loupe/lib/error.js ***!
  \*****************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectObject)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

const errorKeys = [
    'stack',
    'line',
    'column',
    'name',
    'message',
    'fileName',
    'lineNumber',
    'columnNumber',
    'number',
    'description',
    'cause',
];
function inspectObject(error, options) {
    const properties = Object.getOwnPropertyNames(error).filter(key => errorKeys.indexOf(key) === -1);
    const name = error.name;
    options.truncate -= name.length;
    let message = '';
    if (typeof error.message === 'string') {
        message = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(error.message, options.truncate);
    }
    else {
        properties.unshift('message');
    }
    message = message ? `: ${message}` : '';
    options.truncate -= message.length + 5;
    options.seen = options.seen || [];
    if (options.seen.includes(error)) {
        return '[Circular]';
    }
    options.seen.push(error);
    const propertyContents = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(properties.map(key => [key, error[key]]), options, _helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectProperty);
    return `${name}${message}${propertyContents ? ` { ${propertyContents} }` : ''}`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/function.js":
/*!********************************************!*\
  !*** ./node_modules/loupe/lib/function.js ***!
  \********************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectFunction)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectFunction(func, options) {
    const functionType = func[Symbol.toStringTag] || 'Function';
    const name = func.name;
    if (!name) {
        return options.stylize(`[${functionType}]`, 'special');
    }
    return options.stylize(`[${functionType} ${(0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(name, options.truncate - 11)}]`, 'special');
}


/***/ }),

/***/ "./node_modules/loupe/lib/helpers.js":
/*!*******************************************!*\
  !*** ./node_modules/loupe/lib/helpers.js ***!
  \*******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   inspectList: () => (/* binding */ inspectList),
/* harmony export */   inspectProperty: () => (/* binding */ inspectProperty),
/* harmony export */   normaliseOptions: () => (/* binding */ normaliseOptions),
/* harmony export */   truncate: () => (/* binding */ truncate),
/* harmony export */   truncator: () => (/* binding */ truncator)
/* harmony export */ });
const ansiColors = {
    bold: ['1', '22'],
    dim: ['2', '22'],
    italic: ['3', '23'],
    underline: ['4', '24'],
    // 5 & 6 are blinking
    inverse: ['7', '27'],
    hidden: ['8', '28'],
    strike: ['9', '29'],
    // 10-20 are fonts
    // 21-29 are resets for 1-9
    black: ['30', '39'],
    red: ['31', '39'],
    green: ['32', '39'],
    yellow: ['33', '39'],
    blue: ['34', '39'],
    magenta: ['35', '39'],
    cyan: ['36', '39'],
    white: ['37', '39'],
    brightblack: ['30;1', '39'],
    brightred: ['31;1', '39'],
    brightgreen: ['32;1', '39'],
    brightyellow: ['33;1', '39'],
    brightblue: ['34;1', '39'],
    brightmagenta: ['35;1', '39'],
    brightcyan: ['36;1', '39'],
    brightwhite: ['37;1', '39'],
    grey: ['90', '39'],
};
const styles = {
    special: 'cyan',
    number: 'yellow',
    bigint: 'yellow',
    boolean: 'yellow',
    undefined: 'grey',
    null: 'bold',
    string: 'green',
    symbol: 'green',
    date: 'magenta',
    regexp: 'red',
};
const truncator = '…';
function colorise(value, styleType) {
    const color = ansiColors[styles[styleType]] || ansiColors[styleType] || '';
    if (!color) {
        return String(value);
    }
    return `\u001b[${color[0]}m${String(value)}\u001b[${color[1]}m`;
}
function normaliseOptions({ showHidden = false, depth = 2, colors = false, customInspect = true, showProxy = false, maxArrayLength = Infinity, breakLength = Infinity, seen = [], 
// eslint-disable-next-line no-shadow
truncate = Infinity, stylize = String, } = {}, inspect) {
    const options = {
        showHidden: Boolean(showHidden),
        depth: Number(depth),
        colors: Boolean(colors),
        customInspect: Boolean(customInspect),
        showProxy: Boolean(showProxy),
        maxArrayLength: Number(maxArrayLength),
        breakLength: Number(breakLength),
        truncate: Number(truncate),
        seen,
        inspect,
        stylize,
    };
    if (options.colors) {
        options.stylize = colorise;
    }
    return options;
}
function isHighSurrogate(char) {
    return char >= '\ud800' && char <= '\udbff';
}
function truncate(string, length, tail = truncator) {
    string = String(string);
    const tailLength = tail.length;
    const stringLength = string.length;
    if (tailLength > length && stringLength > tailLength) {
        return tail;
    }
    if (stringLength > length && stringLength > tailLength) {
        let end = length - tailLength;
        if (end > 0 && isHighSurrogate(string[end - 1])) {
            end = end - 1;
        }
        return `${string.slice(0, end)}${tail}`;
    }
    return string;
}
// eslint-disable-next-line complexity
function inspectList(list, options, inspectItem, separator = ', ') {
    inspectItem = inspectItem || options.inspect;
    const size = list.length;
    if (size === 0)
        return '';
    const originalLength = options.truncate;
    let output = '';
    let peek = '';
    let truncated = '';
    for (let i = 0; i < size; i += 1) {
        const last = i + 1 === list.length;
        const secondToLast = i + 2 === list.length;
        truncated = `${truncator}(${list.length - i})`;
        const value = list[i];
        // If there is more than one remaining we need to account for a separator of `, `
        options.truncate = originalLength - output.length - (last ? 0 : separator.length);
        const string = peek || inspectItem(value, options) + (last ? '' : separator);
        const nextLength = output.length + string.length;
        const truncatedLength = nextLength + truncated.length;
        // If this is the last element, and adding it would
        // take us over length, but adding the truncator wouldn't - then break now
        if (last && nextLength > originalLength && output.length + truncated.length <= originalLength) {
            break;
        }
        // If this isn't the last or second to last element to scan,
        // but the string is already over length then break here
        if (!last && !secondToLast && truncatedLength > originalLength) {
            break;
        }
        // Peek at the next string to determine if we should
        // break early before adding this item to the output
        peek = last ? '' : inspectItem(list[i + 1], options) + (secondToLast ? '' : separator);
        // If we have one element left, but this element and
        // the next takes over length, the break early
        if (!last && secondToLast && truncatedLength > originalLength && nextLength + peek.length > originalLength) {
            break;
        }
        output += string;
        // If the next element takes us to length -
        // but there are more after that, then we should truncate now
        if (!last && !secondToLast && nextLength + peek.length >= originalLength) {
            truncated = `${truncator}(${list.length - i - 1})`;
            break;
        }
        truncated = '';
    }
    return `${output}${truncated}`;
}
function quoteComplexKey(key) {
    if (key.match(/^[a-zA-Z_][a-zA-Z_0-9]*$/)) {
        return key;
    }
    return JSON.stringify(key)
        .replace(/'/g, "\\'")
        .replace(/\\"/g, '"')
        .replace(/(^"|"$)/g, "'");
}
function inspectProperty([key, value], options) {
    options.truncate -= 2;
    if (typeof key === 'string') {
        key = quoteComplexKey(key);
    }
    else if (typeof key !== 'number') {
        key = `[${options.inspect(key, options)}]`;
    }
    options.truncate -= key.length;
    value = options.inspect(value, options);
    return `${key}: ${value}`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/html.js":
/*!****************************************!*\
  !*** ./node_modules/loupe/lib/html.js ***!
  \****************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectHTML),
/* harmony export */   inspectAttribute: () => (/* binding */ inspectAttribute),
/* harmony export */   inspectNode: () => (/* binding */ inspectNode),
/* harmony export */   inspectNodeCollection: () => (/* binding */ inspectNodeCollection)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectAttribute([key, value], options) {
    options.truncate -= 3;
    if (!value) {
        return `${options.stylize(String(key), 'yellow')}`;
    }
    return `${options.stylize(String(key), 'yellow')}=${options.stylize(`"${value}"`, 'string')}`;
}
function inspectNodeCollection(collection, options) {
    return (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(collection, options, inspectNode, '\n');
}
function inspectNode(node, options) {
    switch (node.nodeType) {
        case 1:
            return inspectHTML(node, options);
        case 3:
            return options.inspect(node.data, options);
        default:
            return options.inspect(node, options);
    }
}
// @ts-ignore (Deno doesn't have Element)
function inspectHTML(element, options) {
    const properties = element.getAttributeNames();
    const name = element.tagName.toLowerCase();
    const head = options.stylize(`<${name}`, 'special');
    const headClose = options.stylize(`>`, 'special');
    const tail = options.stylize(`</${name}>`, 'special');
    options.truncate -= name.length * 2 + 5;
    let propertyContents = '';
    if (properties.length > 0) {
        propertyContents += ' ';
        propertyContents += (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(properties.map((key) => [key, element.getAttribute(key)]), options, inspectAttribute, ' ');
    }
    options.truncate -= propertyContents.length;
    const truncate = options.truncate;
    let children = inspectNodeCollection(element.children, options);
    if (children && children.length > truncate) {
        children = `${_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncator}(${element.children.length})`;
    }
    return `${head}${propertyContents}${headClose}${children}${tail}`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/index.js":
/*!*****************************************!*\
  !*** ./node_modules/loupe/lib/index.js ***!
  \*****************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   custom: () => (/* binding */ custom),
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__),
/* harmony export */   inspect: () => (/* binding */ inspect),
/* harmony export */   registerConstructor: () => (/* binding */ registerConstructor),
/* harmony export */   registerStringTag: () => (/* binding */ registerStringTag)
/* harmony export */ });
/* harmony import */ var _array_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./array.js */ "./node_modules/loupe/lib/array.js");
/* harmony import */ var _typedarray_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./typedarray.js */ "./node_modules/loupe/lib/typedarray.js");
/* harmony import */ var _date_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./date.js */ "./node_modules/loupe/lib/date.js");
/* harmony import */ var _function_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./function.js */ "./node_modules/loupe/lib/function.js");
/* harmony import */ var _map_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./map.js */ "./node_modules/loupe/lib/map.js");
/* harmony import */ var _number_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ./number.js */ "./node_modules/loupe/lib/number.js");
/* harmony import */ var _bigint_js__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ./bigint.js */ "./node_modules/loupe/lib/bigint.js");
/* harmony import */ var _regexp_js__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./regexp.js */ "./node_modules/loupe/lib/regexp.js");
/* harmony import */ var _set_js__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(/*! ./set.js */ "./node_modules/loupe/lib/set.js");
/* harmony import */ var _string_js__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(/*! ./string.js */ "./node_modules/loupe/lib/string.js");
/* harmony import */ var _symbol_js__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(/*! ./symbol.js */ "./node_modules/loupe/lib/symbol.js");
/* harmony import */ var _promise_js__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(/*! ./promise.js */ "./node_modules/loupe/lib/promise.js");
/* harmony import */ var _class_js__WEBPACK_IMPORTED_MODULE_12__ = __webpack_require__(/*! ./class.js */ "./node_modules/loupe/lib/class.js");
/* harmony import */ var _object_js__WEBPACK_IMPORTED_MODULE_13__ = __webpack_require__(/*! ./object.js */ "./node_modules/loupe/lib/object.js");
/* harmony import */ var _arguments_js__WEBPACK_IMPORTED_MODULE_14__ = __webpack_require__(/*! ./arguments.js */ "./node_modules/loupe/lib/arguments.js");
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_15__ = __webpack_require__(/*! ./error.js */ "./node_modules/loupe/lib/error.js");
/* harmony import */ var _html_js__WEBPACK_IMPORTED_MODULE_16__ = __webpack_require__(/*! ./html.js */ "./node_modules/loupe/lib/html.js");
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_17__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");
/* !
 * loupe
 * Copyright(c) 2013 Jake Luer <jake@alogicalparadox.com>
 * MIT Licensed
 */


















const symbolsSupported = typeof Symbol === 'function' && typeof Symbol.for === 'function';
const chaiInspect = symbolsSupported ? Symbol.for('chai/inspect') : '@@chai/inspect';
const nodeInspect = Symbol.for('nodejs.util.inspect.custom');
const constructorMap = new WeakMap();
const stringTagMap = {};
const baseTypesMap = {
    undefined: (value, options) => options.stylize('undefined', 'undefined'),
    null: (value, options) => options.stylize('null', 'null'),
    boolean: (value, options) => options.stylize(String(value), 'boolean'),
    Boolean: (value, options) => options.stylize(String(value), 'boolean'),
    number: _number_js__WEBPACK_IMPORTED_MODULE_5__["default"],
    Number: _number_js__WEBPACK_IMPORTED_MODULE_5__["default"],
    bigint: _bigint_js__WEBPACK_IMPORTED_MODULE_6__["default"],
    BigInt: _bigint_js__WEBPACK_IMPORTED_MODULE_6__["default"],
    string: _string_js__WEBPACK_IMPORTED_MODULE_9__["default"],
    String: _string_js__WEBPACK_IMPORTED_MODULE_9__["default"],
    function: _function_js__WEBPACK_IMPORTED_MODULE_3__["default"],
    Function: _function_js__WEBPACK_IMPORTED_MODULE_3__["default"],
    symbol: _symbol_js__WEBPACK_IMPORTED_MODULE_10__["default"],
    // A Symbol polyfill will return `Symbol` not `symbol` from typedetect
    Symbol: _symbol_js__WEBPACK_IMPORTED_MODULE_10__["default"],
    Array: _array_js__WEBPACK_IMPORTED_MODULE_0__["default"],
    Date: _date_js__WEBPACK_IMPORTED_MODULE_2__["default"],
    Map: _map_js__WEBPACK_IMPORTED_MODULE_4__["default"],
    Set: _set_js__WEBPACK_IMPORTED_MODULE_8__["default"],
    RegExp: _regexp_js__WEBPACK_IMPORTED_MODULE_7__["default"],
    Promise: _promise_js__WEBPACK_IMPORTED_MODULE_11__["default"],
    // WeakSet, WeakMap are totally opaque to us
    WeakSet: (value, options) => options.stylize('WeakSet{…}', 'special'),
    WeakMap: (value, options) => options.stylize('WeakMap{…}', 'special'),
    Arguments: _arguments_js__WEBPACK_IMPORTED_MODULE_14__["default"],
    Int8Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Uint8Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Uint8ClampedArray: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Int16Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Uint16Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Int32Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Uint32Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Float32Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Float64Array: _typedarray_js__WEBPACK_IMPORTED_MODULE_1__["default"],
    Generator: () => '',
    DataView: () => '',
    ArrayBuffer: () => '',
    Error: _error_js__WEBPACK_IMPORTED_MODULE_15__["default"],
    HTMLCollection: _html_js__WEBPACK_IMPORTED_MODULE_16__.inspectNodeCollection,
    NodeList: _html_js__WEBPACK_IMPORTED_MODULE_16__.inspectNodeCollection,
};
// eslint-disable-next-line complexity
const inspectCustom = (value, options, type) => {
    if (chaiInspect in value && typeof value[chaiInspect] === 'function') {
        return value[chaiInspect](options);
    }
    if (nodeInspect in value && typeof value[nodeInspect] === 'function') {
        return value[nodeInspect](options.depth, options);
    }
    if ('inspect' in value && typeof value.inspect === 'function') {
        return value.inspect(options.depth, options);
    }
    if ('constructor' in value && constructorMap.has(value.constructor)) {
        return constructorMap.get(value.constructor)(value, options);
    }
    if (stringTagMap[type]) {
        return stringTagMap[type](value, options);
    }
    return '';
};
const toString = Object.prototype.toString;
// eslint-disable-next-line complexity
function inspect(value, opts = {}) {
    const options = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_17__.normaliseOptions)(opts, inspect);
    const { customInspect } = options;
    let type = value === null ? 'null' : typeof value;
    if (type === 'object') {
        type = toString.call(value).slice(8, -1);
    }
    // If it is a base value that we already support, then use Loupe's inspector
    if (type in baseTypesMap) {
        return baseTypesMap[type](value, options);
    }
    // If `options.customInspect` is set to true then try to use the custom inspector
    if (customInspect && value) {
        const output = inspectCustom(value, options, type);
        if (output) {
            if (typeof output === 'string')
                return output;
            return inspect(output, options);
        }
    }
    const proto = value ? Object.getPrototypeOf(value) : false;
    // If it's a plain Object then use Loupe's inspector
    if (proto === Object.prototype || proto === null) {
        return (0,_object_js__WEBPACK_IMPORTED_MODULE_13__["default"])(value, options);
    }
    // Specifically account for HTMLElements
    // @ts-ignore
    if (value && typeof HTMLElement === 'function' && value instanceof HTMLElement) {
        return (0,_html_js__WEBPACK_IMPORTED_MODULE_16__["default"])(value, options);
    }
    if ('constructor' in value) {
        // If it is a class, inspect it like an object but add the constructor name
        if (value.constructor !== Object) {
            return (0,_class_js__WEBPACK_IMPORTED_MODULE_12__["default"])(value, options);
        }
        // If it is an object with an anonymous prototype, display it as an object.
        return (0,_object_js__WEBPACK_IMPORTED_MODULE_13__["default"])(value, options);
    }
    // last chance to check if it's an object
    if (value === Object(value)) {
        return (0,_object_js__WEBPACK_IMPORTED_MODULE_13__["default"])(value, options);
    }
    // We have run out of options! Just stringify the value
    return options.stylize(String(value), type);
}
function registerConstructor(constructor, inspector) {
    if (constructorMap.has(constructor)) {
        return false;
    }
    constructorMap.set(constructor, inspector);
    return true;
}
function registerStringTag(stringTag, inspector) {
    if (stringTag in stringTagMap) {
        return false;
    }
    stringTagMap[stringTag] = inspector;
    return true;
}
const custom = chaiInspect;
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (inspect);


/***/ }),

/***/ "./node_modules/loupe/lib/map.js":
/*!***************************************!*\
  !*** ./node_modules/loupe/lib/map.js ***!
  \***************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectMap)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectMapEntry([key, value], options) {
    options.truncate -= 4;
    key = options.inspect(key, options);
    options.truncate -= key.length;
    value = options.inspect(value, options);
    return `${key} => ${value}`;
}
// IE11 doesn't support `map.entries()`
function mapToEntries(map) {
    const entries = [];
    map.forEach((value, key) => {
        entries.push([key, value]);
    });
    return entries;
}
function inspectMap(map, options) {
    if (map.size === 0)
        return 'Map{}';
    options.truncate -= 7;
    return `Map{ ${(0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(mapToEntries(map), options, inspectMapEntry)} }`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/number.js":
/*!******************************************!*\
  !*** ./node_modules/loupe/lib/number.js ***!
  \******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectNumber)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

const isNaN = Number.isNaN || (i => i !== i); // eslint-disable-line no-self-compare
function inspectNumber(number, options) {
    if (isNaN(number)) {
        return options.stylize('NaN', 'number');
    }
    if (number === Infinity) {
        return options.stylize('Infinity', 'number');
    }
    if (number === -Infinity) {
        return options.stylize('-Infinity', 'number');
    }
    if (number === 0) {
        return options.stylize(1 / number === Infinity ? '+0' : '-0', 'number');
    }
    return options.stylize((0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(String(number), options.truncate), 'number');
}


/***/ }),

/***/ "./node_modules/loupe/lib/object.js":
/*!******************************************!*\
  !*** ./node_modules/loupe/lib/object.js ***!
  \******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectObject)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectObject(object, options) {
    const properties = Object.getOwnPropertyNames(object);
    const symbols = Object.getOwnPropertySymbols ? Object.getOwnPropertySymbols(object) : [];
    if (properties.length === 0 && symbols.length === 0) {
        return '{}';
    }
    options.truncate -= 4;
    options.seen = options.seen || [];
    if (options.seen.includes(object)) {
        return '[Circular]';
    }
    options.seen.push(object);
    const propertyContents = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(properties.map(key => [key, object[key]]), options, _helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectProperty);
    const symbolContents = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(symbols.map(key => [key, object[key]]), options, _helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectProperty);
    options.seen.pop();
    let sep = '';
    if (propertyContents && symbolContents) {
        sep = ', ';
    }
    return `{ ${propertyContents}${sep}${symbolContents} }`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/promise.js":
/*!*******************************************!*\
  !*** ./node_modules/loupe/lib/promise.js ***!
  \*******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
const getPromiseValue = () => 'Promise{…}';
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (getPromiseValue);


/***/ }),

/***/ "./node_modules/loupe/lib/regexp.js":
/*!******************************************!*\
  !*** ./node_modules/loupe/lib/regexp.js ***!
  \******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectRegExp)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

function inspectRegExp(value, options) {
    const flags = value.toString().split('/')[2];
    const sourceLength = options.truncate - (2 + flags.length);
    const source = value.source;
    return options.stylize(`/${(0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(source, sourceLength)}/${flags}`, 'regexp');
}


/***/ }),

/***/ "./node_modules/loupe/lib/set.js":
/*!***************************************!*\
  !*** ./node_modules/loupe/lib/set.js ***!
  \***************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectSet)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

// IE11 doesn't support `Array.from(set)`
function arrayFromSet(set) {
    const values = [];
    set.forEach(value => {
        values.push(value);
    });
    return values;
}
function inspectSet(set, options) {
    if (set.size === 0)
        return 'Set{}';
    options.truncate -= 7;
    return `Set{ ${(0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(arrayFromSet(set), options)} }`;
}


/***/ }),

/***/ "./node_modules/loupe/lib/string.js":
/*!******************************************!*\
  !*** ./node_modules/loupe/lib/string.js ***!
  \******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectString)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

const stringEscapeChars = new RegExp("['\\u0000-\\u001f\\u007f-\\u009f\\u00ad\\u0600-\\u0604\\u070f\\u17b4\\u17b5" +
    '\\u200c-\\u200f\\u2028-\\u202f\\u2060-\\u206f\\ufeff\\ufff0-\\uffff]', 'g');
const escapeCharacters = {
    '\b': '\\b',
    '\t': '\\t',
    '\n': '\\n',
    '\f': '\\f',
    '\r': '\\r',
    "'": "\\'",
    '\\': '\\\\',
};
const hex = 16;
const unicodeLength = 4;
function escape(char) {
    return (escapeCharacters[char] ||
        `\\u${`0000${char.charCodeAt(0).toString(hex)}`.slice(-unicodeLength)}`);
}
function inspectString(string, options) {
    if (stringEscapeChars.test(string)) {
        string = string.replace(stringEscapeChars, escape);
    }
    return options.stylize(`'${(0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(string, options.truncate - 2)}'`, 'string');
}


/***/ }),

/***/ "./node_modules/loupe/lib/symbol.js":
/*!******************************************!*\
  !*** ./node_modules/loupe/lib/symbol.js ***!
  \******************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectSymbol)
/* harmony export */ });
function inspectSymbol(value) {
    if ('description' in Symbol.prototype) {
        return value.description ? `Symbol(${value.description})` : 'Symbol()';
    }
    return value.toString();
}


/***/ }),

/***/ "./node_modules/loupe/lib/typedarray.js":
/*!**********************************************!*\
  !*** ./node_modules/loupe/lib/typedarray.js ***!
  \**********************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ inspectTypedArray)
/* harmony export */ });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./helpers.js */ "./node_modules/loupe/lib/helpers.js");

const getArrayName = (array) => {
    // We need to special case Node.js' Buffers, which report to be Uint8Array
    // @ts-ignore
    if (typeof Buffer === 'function' && array instanceof Buffer) {
        return 'Buffer';
    }
    if (array[Symbol.toStringTag]) {
        return array[Symbol.toStringTag];
    }
    return array.constructor.name;
};
function inspectTypedArray(array, options) {
    const name = getArrayName(array);
    options.truncate -= name.length + 4;
    // Object.keys will always output the Array indices first, so we can slice by
    // `array.length` to get non-index properties
    const nonIndexProperties = Object.keys(array).slice(array.length);
    if (!array.length && !nonIndexProperties.length)
        return `${name}[]`;
    // As we know TypedArrays only contain Unsigned Integers, we can skip inspecting each one and simply
    // stylise the toString() value of them
    let output = '';
    for (let i = 0; i < array.length; i++) {
        const string = `${options.stylize((0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncate)(array[i], options.truncate), 'number')}${i === array.length - 1 ? '' : ', '}`;
        options.truncate -= string.length;
        if (array[i] !== array.length && options.truncate <= 3) {
            output += `${_helpers_js__WEBPACK_IMPORTED_MODULE_0__.truncator}(${array.length - array[i] + 1})`;
            break;
        }
        output += string;
    }
    let propertyContents = '';
    if (nonIndexProperties.length) {
        propertyContents = (0,_helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectList)(nonIndexProperties.map(key => [key, array[key]]), options, _helpers_js__WEBPACK_IMPORTED_MODULE_0__.inspectProperty);
    }
    return `${name}[ ${output}${propertyContents ? `, ${propertyContents}` : ''} ]`;
}


/***/ }),

/***/ "./node_modules/pathval/index.js":
/*!***************************************!*\
  !*** ./node_modules/pathval/index.js ***!
  \***************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   getPathInfo: () => (/* binding */ getPathInfo),
/* harmony export */   getPathValue: () => (/* binding */ getPathValue),
/* harmony export */   hasProperty: () => (/* binding */ hasProperty),
/* harmony export */   setPathValue: () => (/* binding */ setPathValue)
/* harmony export */ });
/* !
 * Chai - pathval utility
 * Copyright(c) 2012-2014 Jake Luer <jake@alogicalparadox.com>
 * @see https://github.com/logicalparadox/filtr
 * MIT Licensed
 */

/**
 * ### .hasProperty(object, name)
 *
 * This allows checking whether an object has own
 * or inherited from prototype chain named property.
 *
 * Basically does the same thing as the `in`
 * operator but works properly with null/undefined values
 * and other primitives.
 *
 *     var obj = {
 *         arr: ['a', 'b', 'c']
 *       , str: 'Hello'
 *     }
 *
 * The following would be the results.
 *
 *     hasProperty(obj, 'str');  // true
 *     hasProperty(obj, 'constructor');  // true
 *     hasProperty(obj, 'bar');  // false
 *
 *     hasProperty(obj.str, 'length'); // true
 *     hasProperty(obj.str, 1);  // true
 *     hasProperty(obj.str, 5);  // false
 *
 *     hasProperty(obj.arr, 'length');  // true
 *     hasProperty(obj.arr, 2);  // true
 *     hasProperty(obj.arr, 3);  // false
 *
 * @param {Object} object
 * @param {String|Symbol} name
 * @returns {Boolean} whether it exists
 * @namespace Utils
 * @name hasProperty
 * @api public
 */

function hasProperty(obj, name) {
  if (typeof obj === 'undefined' || obj === null) {
    return false;
  }

  // The `in` operator does not work with primitives.
  return name in Object(obj);
}

/* !
 * ## parsePath(path)
 *
 * Helper function used to parse string object
 * paths. Use in conjunction with `internalGetPathValue`.
 *
 *      var parsed = parsePath('myobject.property.subprop');
 *
 * ### Paths:
 *
 * * Can be infinitely deep and nested.
 * * Arrays are also valid using the formal `myobject.document[3].property`.
 * * Literal dots and brackets (not delimiter) must be backslash-escaped.
 *
 * @param {String} path
 * @returns {Object} parsed
 * @api private
 */

function parsePath(path) {
  const str = path.replace(/([^\\])\[/g, '$1.[');
  const parts = str.match(/(\\\.|[^.]+?)+/g);
  return parts.map((value) => {
    if (
      value === 'constructor' ||
      value === '__proto__' ||
      value === 'prototype'
    ) {
      return {};
    }
    const regexp = /^\[(\d+)\]$/;
    const mArr = regexp.exec(value);
    let parsed = null;
    if (mArr) {
      parsed = { i: parseFloat(mArr[1]) };
    } else {
      parsed = { p: value.replace(/\\([.[\]])/g, '$1') };
    }

    return parsed;
  });
}

/* !
 * ## internalGetPathValue(obj, parsed[, pathDepth])
 *
 * Helper companion function for `.parsePath` that returns
 * the value located at the parsed address.
 *
 *      var value = getPathValue(obj, parsed);
 *
 * @param {Object} object to search against
 * @param {Object} parsed definition from `parsePath`.
 * @param {Number} depth (nesting level) of the property we want to retrieve
 * @returns {Object|Undefined} value
 * @api private
 */

function internalGetPathValue(obj, parsed, pathDepth) {
  let temporaryValue = obj;
  let res = null;
  pathDepth = typeof pathDepth === 'undefined' ? parsed.length : pathDepth;

  for (let i = 0; i < pathDepth; i++) {
    const part = parsed[i];
    if (temporaryValue) {
      if (typeof part.p === 'undefined') {
        temporaryValue = temporaryValue[part.i];
      } else {
        temporaryValue = temporaryValue[part.p];
      }

      if (i === pathDepth - 1) {
        res = temporaryValue;
      }
    }
  }

  return res;
}

/* !
 * ## internalSetPathValue(obj, value, parsed)
 *
 * Companion function for `parsePath` that sets
 * the value located at a parsed address.
 *
 *  internalSetPathValue(obj, 'value', parsed);
 *
 * @param {Object} object to search and define on
 * @param {*} value to use upon set
 * @param {Object} parsed definition from `parsePath`
 * @api private
 */

function internalSetPathValue(obj, val, parsed) {
  let tempObj = obj;
  const pathDepth = parsed.length;
  let part = null;
  // Here we iterate through every part of the path
  for (let i = 0; i < pathDepth; i++) {
    let propName = null;
    let propVal = null;
    part = parsed[i];

    // If it's the last part of the path, we set the 'propName' value with the property name
    if (i === pathDepth - 1) {
      propName = typeof part.p === 'undefined' ? part.i : part.p;
      // Now we set the property with the name held by 'propName' on object with the desired val
      tempObj[propName] = val;
    } else if (typeof part.p !== 'undefined' && tempObj[part.p]) {
      tempObj = tempObj[part.p];
    } else if (typeof part.i !== 'undefined' && tempObj[part.i]) {
      tempObj = tempObj[part.i];
    } else {
      // If the obj doesn't have the property we create one with that name to define it
      const next = parsed[i + 1];
      // Here we set the name of the property which will be defined
      propName = typeof part.p === 'undefined' ? part.i : part.p;
      // Here we decide if this property will be an array or a new object
      propVal = typeof next.p === 'undefined' ? [] : {};
      tempObj[propName] = propVal;
      tempObj = tempObj[propName];
    }
  }
}

/**
 * ### .getPathInfo(object, path)
 *
 * This allows the retrieval of property info in an
 * object given a string path.
 *
 * The path info consists of an object with the
 * following properties:
 *
 * * parent - The parent object of the property referenced by `path`
 * * name - The name of the final property, a number if it was an array indexer
 * * value - The value of the property, if it exists, otherwise `undefined`
 * * exists - Whether the property exists or not
 *
 * @param {Object} object
 * @param {String} path
 * @returns {Object} info
 * @namespace Utils
 * @name getPathInfo
 * @api public
 */

function getPathInfo(obj, path) {
  const parsed = parsePath(path);
  const last = parsed[parsed.length - 1];
  const info = {
    parent:
      parsed.length > 1 ?
        internalGetPathValue(obj, parsed, parsed.length - 1) :
        obj,
    name: last.p || last.i,
    value: internalGetPathValue(obj, parsed),
  };
  info.exists = hasProperty(info.parent, info.name);

  return info;
}

/**
 * ### .getPathValue(object, path)
 *
 * This allows the retrieval of values in an
 * object given a string path.
 *
 *     var obj = {
 *         prop1: {
 *             arr: ['a', 'b', 'c']
 *           , str: 'Hello'
 *         }
 *       , prop2: {
 *             arr: [ { nested: 'Universe' } ]
 *           , str: 'Hello again!'
 *         }
 *     }
 *
 * The following would be the results.
 *
 *     getPathValue(obj, 'prop1.str'); // Hello
 *     getPathValue(obj, 'prop1.att[2]'); // b
 *     getPathValue(obj, 'prop2.arr[0].nested'); // Universe
 *
 * @param {Object} object
 * @param {String} path
 * @returns {Object} value or `undefined`
 * @namespace Utils
 * @name getPathValue
 * @api public
 */

function getPathValue(obj, path) {
  const info = getPathInfo(obj, path);
  return info.value;
}

/**
 * ### .setPathValue(object, path, value)
 *
 * Define the value in an object at a given string path.
 *
 * ```js
 * var obj = {
 *     prop1: {
 *         arr: ['a', 'b', 'c']
 *       , str: 'Hello'
 *     }
 *   , prop2: {
 *         arr: [ { nested: 'Universe' } ]
 *       , str: 'Hello again!'
 *     }
 * };
 * ```
 *
 * The following would be acceptable.
 *
 * ```js
 * var properties = require('tea-properties');
 * properties.set(obj, 'prop1.str', 'Hello Universe!');
 * properties.set(obj, 'prop1.arr[2]', 'B');
 * properties.set(obj, 'prop2.arr[0].nested.value', { hello: 'universe' });
 * ```
 *
 * @param {Object} object
 * @param {String} path
 * @param {Mixed} value
 * @api private
 */

function setPathValue(obj, path, val) {
  const parsed = parsePath(path);
  internalSetPathValue(obj, val, parsed);
  return obj;
}


/***/ })

/******/ 	});
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		var cachedModule = __webpack_module_cache__[moduleId];
/******/ 		if (cachedModule !== undefined) {
/******/ 			return cachedModule.exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			// no module.id needed
/******/ 			// no module.loaded needed
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId](module, module.exports, __webpack_require__);
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/************************************************************************/
/******/ 	/* webpack/runtime/define property getters */
/******/ 	(() => {
/******/ 		// define getter functions for harmony exports
/******/ 		__webpack_require__.d = (exports, definition) => {
/******/ 			for(var key in definition) {
/******/ 				if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 					Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 				}
/******/ 			}
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/make namespace object */
/******/ 	(() => {
/******/ 		// define __esModule on exports
/******/ 		__webpack_require__.r = (exports) => {
/******/ 			if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 				Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 			}
/******/ 			Object.defineProperty(exports, '__esModule', { value: true });
/******/ 		};
/******/ 	})();
/******/ 	
/************************************************************************/
var __webpack_exports__ = {};
// This entry needs to be wrapped in an IIFE because it needs to be isolated against other modules in the chunk.
(() => {
/*!**********************!*\
  !*** ./src/chai.mjs ***!
  \**********************/
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   runTest: () => (/* binding */ runTest)
/* harmony export */ });
/* harmony import */ var chai__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! chai */ "./node_modules/chai/lib/chai.js");
// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.




const assert = chai__WEBPACK_IMPORTED_MODULE_0__.assert;
const expect = chai__WEBPACK_IMPORTED_MODULE_0__.expect;
const AssertionError = chai__WEBPACK_IMPORTED_MODULE_0__.AssertionError;

const tests = [];

const describe = (name, func) => func();
const it = (name, func) => tests.push({ name, func });

describe("assert", () => {
  it("assert", () => {
    const foo = "bar";
    assert(foo == "bar", "expected foo to equal `bar`");

    expect(() => {
      assert(foo == "baz", "expected foo to equal `baz`");
    }).to.throw(AssertionError, "expected foo to equal `baz`");

    expect(() => {
      assert(foo == "baz", () => "expected foo to equal `baz`");
    }).to.throw(AssertionError, "expected foo to equal `baz`");
  });

  it("fail", () => {
    expect(() => {
      assert.fail(0, 1, "this has failed");
    }).to.throw(AssertionError, "this has failed");
  });

  it("isTrue", () => {
    assert.isTrue(true);

    expect(() => {
      assert.isTrue(false, "blah");
    }).to.throw(AssertionError, "blah: expected false to be true");

    expect(() => {
      assert.isTrue(1);
    }).to.throw(AssertionError, "expected 1 to be true");

    expect(() => {
      assert.isTrue("test");
    }).to.throw(AssertionError, "expected 'test' to be true");
  });

  it("isNotTrue", () => {
    assert.isNotTrue(false);

    expect(() => {
      assert.isNotTrue(true, "blah");
    }).to.throw(AssertionError, "blah: expected true to not equal true");
  });

  it("isOk / ok", () => {
    ["isOk", "ok"].forEach((isOk) => {
      assert[isOk](true);
      assert[isOk](1);
      assert[isOk]("test");

      expect(() => {
        assert[isOk](false, "blah");
      }).to.throw(AssertionError, "blah: expected false to be truthy");

      expect(() => {
        assert[isOk](0);
      }).to.throw(AssertionError, "expected +0 to be truthy");

      expect(() => {
        assert[isOk]("");
      }).to.throw(AssertionError, "expected '' to be truthy");
    });
  });

  it("isNotOk / notOk", () => {
    ["isNotOk", "notOk"].forEach((isNotOk) => {
      assert[isNotOk](false);
      assert[isNotOk](0);
      assert[isNotOk]("");

      expect(() => {
        assert[isNotOk](true, "blah");
      }).to.throw(AssertionError, "blah: expected true to be falsy");

      expect(() => {
        assert[isNotOk](1);
      }).to.throw(AssertionError, "expected 1 to be falsy");

      expect(() => {
        assert[isNotOk]("test");
      }).to.throw(AssertionError, "expected 'test' to be falsy");
    });
  });

  it("isFalse", () => {
    assert.isFalse(false);

    expect(() => {
      assert.isFalse(true, "blah");
    }).to.throw(AssertionError, "blah: expected true to be false");

    expect(() => {
      assert.isFalse(0);
    }).to.throw(AssertionError, "expected +0 to be false");
  });

  it("isNotFalse", () => {
    assert.isNotFalse(true);

    expect(() => {
      assert.isNotFalse(false, "blah");
    }).to.throw(AssertionError, "blah: expected false to not equal false");
  });

  const sym = Symbol();

  it("isEqual", () => {
    assert.equal(0, 0);
    assert.equal(sym, sym);
    assert.equal("test", "test");
    assert.equal(void 0, null);
    assert.equal(void 0, undefined);

    expect(() => {
      assert.equal(NaN, NaN);
    }).to.throw(AssertionError, "expected NaN to equal NaN");

    expect(() => {
      assert.equal(1, 2, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to equal 2");
  });

  it("notEqual", () => {
    assert.notEqual(1, 2);
    assert.notEqual(NaN, NaN);
    assert.notEqual(1, "test");

    expect(() => {
      assert.notEqual("test", "test");
    }).to.throw(AssertionError, "expected 'test' to not equal 'test'");
    expect(() => {
      assert.notEqual(sym, sym);
    }).to.throw(AssertionError, "expected Symbol() to not equal Symbol()");
  });

  it("strictEqual", () => {
    assert.strictEqual(0, 0);
    assert.strictEqual(0, -0);
    assert.strictEqual("foo", "foo");
    assert.strictEqual(sym, sym);

    expect(() => {
      assert.strictEqual("5", 5, "blah");
    }).to.throw(AssertionError, "blah: expected '5' to equal 5");
  });

  it("notStrictEqual", () => {
    assert.notStrictEqual(5, "5");
    assert.notStrictEqual(NaN, NaN);
    assert.notStrictEqual(Symbol(), Symbol());

    expect(() => {
      assert.notStrictEqual(5, 5, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to not equal 5");
  });

  it("deepEqual", () => {
    const obja = Object.create({ tea: "chai" });
    const objb = Object.create({ tea: "chai" });

    assert.deepEqual(/a/, /a/);
    assert.deepEqual(/a/g, /a/g);
    assert.deepEqual(/a/i, /a/i);
    assert.deepEqual(/a/m, /a/m);
    assert.deepEqual(obja, objb);
    assert.deepEqual([NaN], [NaN]);
    assert.deepEqual({ tea: NaN }, { tea: NaN });
    assert.deepEqual({ tea: "chai" }, { tea: "chai" });
    assert.deepEqual({ a: "a", b: "b" }, { b: "b", a: "a" });
    assert.deepEqual(new Date(1, 2, 3), new Date(1, 2, 3));

    expect(() => {
      assert.deepEqual({ tea: "chai" }, { tea: "black" });
    }).to.throw(AssertionError);

    const obj1 = Object.create({ tea: "chai" });
    const obj2 = Object.create({ tea: "black" });

    expect(() => {
      assert.deepEqual(obj1, obj2);
    }).to.throw(AssertionError);

    const circularObject = {};
    const secondCircularObject = {};
    circularObject.field = circularObject;
    secondCircularObject.field = secondCircularObject;

    assert.deepEqual(circularObject, secondCircularObject);

    expect(() => {
      secondCircularObject.field2 = secondCircularObject;
      assert.deepEqual(circularObject, secondCircularObject);
    }).to.throw(AssertionError);
  });

  it("notDeepEqual", () => {
    assert.notDeepEqual({ tea: "jasmine" }, { tea: "chai" });
    assert.notDeepEqual(/a/, /b/);
    assert.notDeepEqual(/a/, {});
    assert.notDeepEqual(/a/g, /b/g);
    assert.notDeepEqual(/a/i, /b/i);
    assert.notDeepEqual(/a/m, /b/m);
    assert.notDeepEqual(new Date(1, 2, 3), new Date(4, 5, 6));
    assert.notDeepEqual(new Date(1, 2, 3), {});

    expect(() => {
      assert.notDeepEqual({ tea: "chai" }, { tea: "chai" });
    }).to.throw(AssertionError);

    const circularObject = {};
    const secondCircularObject = { tea: "jasmine" };
    circularObject.field = circularObject;
    secondCircularObject.field = secondCircularObject;

    assert.notDeepEqual(circularObject, secondCircularObject);

    expect(() => {
      delete secondCircularObject.tea;
      assert.notDeepEqual(circularObject, secondCircularObject);
    }).to.throw(AssertionError);
  });

  it("typeOf", () => {
    assert.typeOf("test", "string");
    assert.typeOf(true, "boolean");
    assert.typeOf(NaN, "number");
    assert.typeOf(sym, "symbol");

    expect(() => {
      assert.typeOf(5, "string", "blah");
    }).to.throw(AssertionError, "blah: expected 5 to be a string");
  });

  it("notTypeOf", () => {
    assert.notTypeOf(5, "string");
    assert.notTypeOf(sym, "string");
    assert.notTypeOf(null, "object");
    assert.notTypeOf("test", "number");

    expect(() => {
      assert.notTypeOf(5, "number", "blah");
    }).to.throw(AssertionError, "blah: expected 5 not to be a number");
  });

  function Foo() {}

  const FakeConstructor = {
    [Symbol.hasInstance](x) {
      return x === 3;
    },
  };

  it("instanceOf", () => {
    assert.instanceOf({}, Object);
    assert.instanceOf(/a/, RegExp);
    assert.instanceOf(new Foo(), Foo);
    assert.instanceOf(3, FakeConstructor);

    expect(() => {
      assert.instanceOf(new Foo(), 1);
    }).to.throw(
      "The instanceof assertion needs a constructor but Number was given."
    );

    expect(() => {
      assert.instanceOf(new Foo(), "Foo");
    }).to.throw(
      "The instanceof assertion needs a constructor but String was given."
    );

    expect(() => {
      assert.instanceOf(4, FakeConstructor);
    }).to.throw("expected 4 to be an instance of an unnamed constructor");
  });

  it("notInstanceOf", () => {
    assert.notInstanceOf({}, Foo);
    assert.notInstanceOf({}, Array);
    assert.notInstanceOf(new Foo(), Array);

    expect(() => {
      assert.notInstanceOf(new Foo(), Foo);
    }).to.throw("expected Foo{} to not be an instance of Foo");

    expect(() => {
      assert.notInstanceOf(3, FakeConstructor);
    }).to.throw("expected 3 to not be an instance of an unnamed constructor");
  });

  it("isObject", () => {
    assert.isObject({});
    assert.isObject(new Foo());

    expect(() => {
      assert.isObject(true);
    }).to.throw(AssertionError, "expected true to be an object");

    expect(() => {
      assert.isObject(Foo);
    }).to.throw(AssertionError, "expected [Function Foo] to be an object");

    expect(() => {
      assert.isObject("foo");
    }).to.throw(AssertionError, "expected 'foo' to be an object");
  });

  it("isNotObject", () => {
    assert.isNotObject(1);
    assert.isNotObject([]);
    assert.isNotObject(/a/);
    assert.isNotObject(Foo);
    assert.isNotObject("foo");

    expect(() => {
      assert.isNotObject({}, "blah");
    }).to.throw(AssertionError, "blah: expected {} not to be an object");
  });

  it("include", () => {
    assert.include("foobar", "bar");
    assert.include("", "");
    assert.include([1, 2, 3], 3);

    // .include should work with Error objects and objects with a custom
    // `@@toStringTag`.
    assert.include(new Error("foo"), { message: "foo" });
    assert.include({ a: 1, [Symbol.toStringTag]: "foo" }, { a: 1 });

    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    assert.include([obj1, obj2], obj1);
    assert.include({ foo: obj1, bar: obj2 }, { foo: obj1 });
    assert.include({ foo: obj1, bar: obj2 }, { foo: obj1, bar: obj2 });

    var map = new Map();
    var val = [{ a: 1 }];
    map.set("a", val);
    map.set("b", 2);
    map.set("c", -0);
    map.set("d", NaN);

    assert.include(map, val);
    assert.include(map, 2);
    assert.include(map, 0);
    assert.include(map, NaN);

    var set = new Set();
    var val = [{ a: 1 }];
    set.add(val);
    set.add(2);
    set.add(-0);
    set.add(NaN);

    assert.include(set, val);
    assert.include(set, 2);
    assert.include(set, NaN);

    var ws = new WeakSet();
    var val = [{ a: 1 }];
    ws.add(val);

    assert.include(ws, val);

    var sym1 = Symbol(),
      sym2 = Symbol();
    assert.include([sym1, sym2], sym1);

    expect(() => {
      assert.include("foobar", "baz", "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to include 'baz'");

    expect(() => {
      assert.include([{ a: 1 }, { b: 2 }], { a: 1 });
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 } ] to include { a: 1 }"
    );

    expect(() => {
      assert.include(
        { foo: { a: 1 }, bar: { b: 2 } },
        { foo: { a: 1 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to have property 'foo' of { a: 1 }, but got { a: 1 }"
    );

    expect(() => {
      assert.include(true, true, "blah");
    }).to.throw(
      AssertionError,
      "blah: the given combination of arguments (boolean and boolean) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a boolean"
    );

    expect(() => {
      assert.include(42, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (number and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );

    expect(() => {
      assert.include(null, 42);
    }).to.throw(
      AssertionError,
      "the given combination of arguments (null and number) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a number"
    );

    expect(() => {
      assert.include(undefined, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (undefined and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );
  });

  it("notInclude", () => {
    assert.notInclude("foobar", "baz");
    assert.notInclude([1, 2, 3], 4);

    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    assert.notInclude([obj1, obj2], { a: 1 });
    assert.notInclude({ foo: obj1, bar: obj2 }, { foo: { a: 1 } });
    assert.notInclude({ foo: obj1, bar: obj2 }, { foo: obj1, bar: { b: 2 } });

    var map = new Map();
    var val = [{ a: 1 }];
    map.set("a", val);
    map.set("b", 2);

    assert.notInclude(map, [{ a: 1 }]);
    assert.notInclude(map, 3);

    var set = new Set();
    var val = [{ a: 1 }];
    set.add(val);
    set.add(2);

    assert.include(set, val);
    assert.include(set, 2);

    assert.notInclude(set, [{ a: 1 }]);
    assert.notInclude(set, 3);

    var ws = new WeakSet();
    var val = [{ a: 1 }];
    ws.add(val);

    assert.notInclude(ws, [{ a: 1 }]);
    assert.notInclude(ws, {});

    var sym1 = Symbol(),
      sym2 = Symbol(),
      sym3 = Symbol();
    assert.notInclude([sym1, sym2], sym3);

    expect(() => {
      var obj1 = { a: 1 },
        obj2 = { b: 2 };
      assert.notInclude([obj1, obj2], obj1, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 } ] to not include { a: 1 }"
    );

    expect(() => {
      var obj1 = { a: 1 },
        obj2 = { b: 2 };
      assert.notInclude(
        { foo: obj1, bar: obj2 },
        { foo: obj1, bar: obj2 },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to not have property 'foo' of { a: 1 }"
    );

    expect(() => {
      assert.notInclude(true, true, "blah");
    }).to.throw(
      AssertionError,
      "blah: the given combination of arguments (boolean and boolean) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a boolean"
    );

    expect(() => {
      assert.notInclude(42, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (number and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );

    expect(() => {
      assert.notInclude(null, 42);
    }).to.throw(
      AssertionError,
      "the given combination of arguments (null and number) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a number"
    );

    expect(() => {
      assert.notInclude(undefined, "bar");
    }).to.throw(
      AssertionError,
      "the given combination of arguments (undefined and string) is invalid for this assertion. You can use an array, a map, an object, a set, a string, or a weakset instead of a string"
    );

    expect(() => {
      assert.notInclude("foobar", "bar");
    }).to.throw(AssertionError, "expected 'foobar' to not include 'bar'");
  });

  it("deepInclude and notDeepInclude", () => {
    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    assert.deepInclude([obj1, obj2], { a: 1 });
    assert.notDeepInclude([obj1, obj2], { a: 9 });
    assert.notDeepInclude([obj1, obj2], { z: 1 });
    assert.deepInclude({ foo: obj1, bar: obj2 }, { foo: { a: 1 } });
    assert.deepInclude(
      { foo: obj1, bar: obj2 },
      { foo: { a: 1 }, bar: { b: 2 } }
    );
    assert.notDeepInclude({ foo: obj1, bar: obj2 }, { foo: { a: 9 } });
    assert.notDeepInclude({ foo: obj1, bar: obj2 }, { foo: { z: 1 } });
    assert.notDeepInclude({ foo: obj1, bar: obj2 }, { baz: { a: 1 } });
    assert.notDeepInclude(
      { foo: obj1, bar: obj2 },
      { foo: { a: 1 }, bar: { b: 9 } }
    );

    var map = new Map();
    map.set(1, [{ a: 1 }]);

    assert.deepInclude(map, [{ a: 1 }]);

    var set = new Set();
    set.add([{ a: 1 }]);

    assert.deepInclude(set, [{ a: 1 }]);

    expect(() => {
      assert.deepInclude(new WeakSet(), {}, "foo");
    }).to.throw(
      AssertionError,
      "foo: unable to use .deep.include with WeakSet"
    );

    expect(() => {
      assert.deepInclude([obj1, obj2], { a: 9 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 } ] to deep include { a: 9 }"
    );

    expect(() => {
      assert.notDeepInclude([obj1, obj2], { a: 1 });
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 } ] to not deep include { a: 1 }"
    );

    expect(() => {
      assert.deepInclude(
        { foo: obj1, bar: obj2 },
        { foo: { a: 1 }, bar: { b: 9 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to have deep property 'bar' of { b: 9 }, but got { b: 2 }"
    );

    expect(() => {
      assert.notDeepInclude(
        { foo: obj1, bar: obj2 },
        { foo: { a: 1 }, bar: { b: 2 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { foo: { a: 1 }, bar: { b: 2 } } to not have deep property 'foo' of { a: 1 }"
    );
  });

  it("nestedInclude and notNestedInclude", () => {
    assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "y" });
    assert.notNestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "x" });
    assert.notNestedInclude({ a: { b: ["x", "y"] } }, { "a.c": "y" });

    assert.notNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.b[0]": { x: 1 } });

    assert.nestedInclude({ ".a": { "[b]": "x" } }, { "\\.a.\\[b\\]": "x" });
    assert.notNestedInclude({ ".a": { "[b]": "x" } }, { "\\.a.\\[b\\]": "y" });

    expect(() => {
      assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "x" }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ 'x', 'y' ] } } to have nested property 'a.b[1]' of 'x', but got 'y'"
    );

    expect(() => {
      assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.b[1]": "x" }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ 'x', 'y' ] } } to have nested property 'a.b[1]' of 'x', but got 'y'"
    );

    expect(() => {
      assert.nestedInclude({ a: { b: ["x", "y"] } }, { "a.c": "y" });
    }).to.throw(
      AssertionError,
      "expected { a: { b: [ 'x', 'y' ] } } to have nested property 'a.c'"
    );

    expect(() => {
      assert.notNestedInclude(
        { a: { b: ["x", "y"] } },
        { "a.b[1]": "y" },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ 'x', 'y' ] } } to not have nested property 'a.b[1]' of 'y'"
    );
  });

  it("deepNestedInclude and notDeepNestedInclude", () => {
    assert.deepNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.b[0]": { x: 1 } });
    assert.notDeepNestedInclude(
      { a: { b: [{ x: 1 }] } },
      { "a.b[0]": { y: 2 } }
    );
    assert.notDeepNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.c": { x: 1 } });

    assert.deepNestedInclude(
      { ".a": { "[b]": { x: 1 } } },
      { "\\.a.\\[b\\]": { x: 1 } }
    );
    assert.notDeepNestedInclude(
      { ".a": { "[b]": { x: 1 } } },
      { "\\.a.\\[b\\]": { y: 2 } }
    );

    expect(() => {
      assert.deepNestedInclude(
        { a: { b: [{ x: 1 }] } },
        { "a.b[0]": { y: 2 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ { x: 1 } ] } } to have deep nested property 'a.b[0]' of { y: 2 }, but got { x: 1 }"
    );

    expect(() => {
      assert.deepNestedInclude(
        { a: { b: [{ x: 1 }] } },
        { "a.b[0]": { y: 2 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ { x: 1 } ] } } to have deep nested property 'a.b[0]' of { y: 2 }, but got { x: 1 }"
    );

    expect(() => {
      assert.deepNestedInclude({ a: { b: [{ x: 1 }] } }, { "a.c": { x: 1 } });
    }).to.throw(
      AssertionError,
      "expected { a: { b: [ { x: 1 } ] } } to have deep nested property 'a.c'"
    );

    expect(() => {
      assert.notDeepNestedInclude(
        { a: { b: [{ x: 1 }] } },
        { "a.b[0]": { x: 1 } },
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: [ { x: 1 } ] } } to not have deep nested property 'a.b[0]' of { x: 1 }"
    );
  });

  it("ownInclude and notOwnInclude", () => {
    assert.ownInclude({ a: 1 }, { a: 1 });
    assert.notOwnInclude({ a: 1 }, { a: 3 });
    assert.notOwnInclude({ a: 1 }, { toString: Object.prototype.toString });

    assert.notOwnInclude({ a: { b: 2 } }, { a: { b: 2 } });

    expect(() => {
      assert.ownInclude({ a: 1 }, { a: 3 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: 1 } to have own property 'a' of 3, but got 1"
    );

    expect(() => {
      assert.ownInclude({ a: 1 }, { a: 3 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: 1 } to have own property 'a' of 3, but got 1"
    );

    expect(() => {
      assert.ownInclude({ a: 1 }, { toString: Object.prototype.toString });
    }).to.throw(
      AssertionError,
      "expected { a: 1 } to have own property 'toString'"
    );

    expect(() => {
      assert.notOwnInclude({ a: 1 }, { a: 1 }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: 1 } to not have own property 'a' of 1"
    );
  });

  it("deepOwnInclude and notDeepOwnInclude", () => {
    assert.deepOwnInclude({ a: { b: 2 } }, { a: { b: 2 } });
    assert.notDeepOwnInclude({ a: { b: 2 } }, { a: { c: 3 } });
    assert.notDeepOwnInclude(
      { a: { b: 2 } },
      { toString: Object.prototype.toString }
    );

    expect(() => {
      assert.deepOwnInclude({ a: { b: 2 } }, { a: { c: 3 } }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: 2 } } to have deep own property 'a' of { c: 3 }, but got { b: 2 }"
    );

    expect(() => {
      assert.deepOwnInclude({ a: { b: 2 } }, { a: { c: 3 } }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: 2 } } to have deep own property 'a' of { c: 3 }, but got { b: 2 }"
    );

    expect(() => {
      assert.deepOwnInclude(
        { a: { b: 2 } },
        { toString: Object.prototype.toString }
      );
    }).to.throw(
      AssertionError,
      "expected { a: { b: 2 } } to have deep own property 'toString'"
    );

    expect(() => {
      assert.notDeepOwnInclude({ a: { b: 2 } }, { a: { b: 2 } }, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected { a: { b: 2 } } to not have deep own property 'a' of { b: 2 }"
    );
  });

  it("lengthOf", () => {
    assert.lengthOf([1, 2, 3], 3);
    assert.lengthOf("foobar", 6);

    expect(() => {
      assert.lengthOf("foobar", 5, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foobar' to have a length of 5 but got 6"
    );

    expect(() => {
      assert.lengthOf(1, 5);
    }).to.throw(AssertionError, "expected 1 to have property 'length'");
  });

  it("match", () => {
    assert.match("foobar", /^foo/);
    assert.notMatch("foobar", /^bar/);

    expect(() => {
      assert.match("foobar", /^bar/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      assert.notMatch("foobar", /^foo/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' not to match /^foo/i");
  });
});

describe("expect", () => {
  const sym = Symbol();

  describe("proxify", () => {
    it("throws when invalid property follows expect", function () {
      expect(() => {
        expect(42).pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows language chain", function () {
      expect(() => {
        expect(42).to.pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows property assertion", function () {
      expect(() => {
        expect(42).ok.pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows uncalled method assertion", function () {
      expect(() => {
        expect(42).equal.pizza;
      }).to.throw(
        Error,
        'Invalid Chai property: equal.pizza. See docs for proper usage of "equal".'
      );
    });

    it("throws when invalid property follows called method assertion", function () {
      expect(() => {
        expect(42).equal(42).pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows uncalled chainable method assertion", function () {
      expect(() => {
        expect(42).a.pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("throws when invalid property follows called chainable method assertion", function () {
      expect(() => {
        expect(42).a("number").pizza;
      }).to.throw(Error, "Invalid Chai property: pizza");
    });

    it("doesn't throw if invalid property is excluded via config", function () {
      expect(() => {
        expect(42).then;
      }).to.not.throw();
    });
  });

  it("no-op chains", () => {
    [
      "to",
      "be",
      "been",
      "is",
      "and",
      "has",
      "have",
      "with",
      "that",
      "which",
      "at",
      "of",
      "same",
      "but",
      "does",
    ].forEach((chain) => {
      // tests that chain exists
      expect(expect(1)[chain]).not.undefined;

      // tests methods
      expect(1)[chain].equal(1);

      // tests properties that assert
      expect(false)[chain].false;

      // tests not
      expect(false)[chain].not.true;

      // tests chainable methods
      expect([1, 2, 3])[chain].contains(1);
    });
  });

  it("fail", () => {
    expect(() => {
      expect.fail(0, 1, "this has failed");
    }).to.throw(AssertionError, "this has failed");
  });

  it("true", () => {
    expect(true).to.be.true;
    expect(false).to.not.be.true;
    expect(1).to.not.be.true;

    expect(() => {
      expect("test", "blah").to.be.true;
    }).to.throw(AssertionError, "blah: expected 'test' to be true");
  });

  it("ok", () => {
    expect(true).to.be.ok;
    expect(false).to.not.be.ok;
    expect(1).to.be.ok;
    expect(0).to.not.be.ok;

    expect(() => {
      expect("", "blah").to.be.ok;
    }).to.throw(AssertionError, "blah: expected '' to be truthy");

    expect(() => {
      expect("test").to.not.be.ok;
    }).to.throw(AssertionError, "expected 'test' to be falsy");
  });

  it("false", () => {
    expect(false).to.be.false;
    expect(true).to.not.be.false;
    expect(0).to.not.be.false;

    expect(() => {
      expect("", "blah").to.be.false;
    }).to.throw(AssertionError, "blah: expected '' to be false");
  });

  it("null", () => {
    expect(null).to.be.null;
    expect(false).to.not.be.null;

    expect(() => {
      expect("", "blah").to.be.null;
    }).to.throw(AssertionError, "blah: expected '' to be null");
  });

  it("undefined", () => {
    expect(undefined).to.be.undefined;
    expect(null).to.not.be.undefined;

    expect(() => {
      expect("", "blah").to.be.undefined;
    }).to.throw(AssertionError, "blah: expected '' to be undefined");
  });

  it("exist", () => {
    const foo = "bar";
    var bar;

    expect(foo).to.exist;
    expect(bar).to.not.exist;
    expect(0).to.exist;
    expect(false).to.exist;
    expect("").to.exist;

    expect(() => {
      expect(bar, "blah").to.exist;
    }).to.throw(AssertionError, "blah: expected undefined to exist");

    expect(() => {
      expect(foo).to.not.exist(foo);
    }).to.throw(AssertionError, "expected 'bar' to not exist");
  });

  it("arguments", () => {
    var args = (function () {
      return arguments;
    })(1, 2, 3);
    expect(args).to.be.arguments;
    expect([]).to.not.be.arguments;
    expect(args).to.be.an("arguments").and.be.arguments;
    expect([]).to.be.an("array").and.not.be.Arguments;

    expect(() => {
      expect([]).to.be.arguments;
    }).to.throw(AssertionError, "expected [] to be arguments but got Array");
  });

  it("instanceof", () => {
    function Foo() {}
    expect(new Foo()).to.be.an.instanceof(Foo);

    expect(() => {
      expect(new Foo()).to.an.instanceof(1, "blah");
    }).to.throw(
      AssertionError,
      "blah: The instanceof assertion needs a constructor but Number was given."
    );

    expect(() => {
      expect(new Foo(), "blah").to.an.instanceof(1);
    }).to.throw(
      AssertionError,
      "blah: The instanceof assertion needs a constructor but Number was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof("batman");
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but String was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof({});
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Object was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(true);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Boolean was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(null);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but null was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(undefined);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but undefined was given."
    );

    expect(() => {
      function Thing() {}
      var t = new Thing();
      Thing.prototype = 1337;
      expect(t).to.an.instanceof(Thing);
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Function was given."
    );

    expect(() => {
      expect(new Foo()).to.an.instanceof(Symbol());
    }).to.throw(
      AssertionError,
      "The instanceof assertion needs a constructor but Symbol was given."
    );

    expect(() => {
      var FakeConstructor = {};
      var fakeInstanceB = 4;
      FakeConstructor[Symbol.hasInstance] = function (val) {
        return val === 3;
      };
      expect(fakeInstanceB).to.be.an.instanceof(FakeConstructor);
    }).to.throw(
      AssertionError,
      "expected 4 to be an instance of an unnamed constructor"
    );

    expect(() => {
      var FakeConstructor = {};
      var fakeInstanceB = 4;
      FakeConstructor[Symbol.hasInstance] = function (val) {
        return val === 4;
      };
      expect(fakeInstanceB).to.not.be.an.instanceof(FakeConstructor);
    }).to.throw(
      AssertionError,
      "expected 4 to not be an instance of an unnamed constructor"
    );

    expect(() => {
      expect(3).to.an.instanceof(Foo, "blah");
    }).to.throw(AssertionError, "blah: expected 3 to be an instance of Foo");

    expect(() => {
      expect(3, "blah").to.an.instanceof(Foo);
    }).to.throw(AssertionError, "blah: expected 3 to be an instance of Foo");
  });

  it("within(start, finish)", () => {
    expect(5).to.be.within(5, 10);
    expect(5).to.be.within(3, 6);
    expect(5).to.be.within(3, 5);
    expect(5).to.not.be.within(1, 3);
    expect("foo").to.have.length.within(2, 4);
    expect("foo").to.have.lengthOf.within(2, 4);
    expect([1, 2, 3]).to.have.length.within(2, 4);
    expect([1, 2, 3]).to.have.lengthOf.within(2, 4);

    expect(() => {
      expect(5).to.not.be.within(4, 6, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to not be within 4..6");

    expect(() => {
      expect(5, "blah").to.not.be.within(4, 6);
    }).to.throw(AssertionError, "blah: expected 5 to not be within 4..6");

    expect(() => {
      expect(10).to.be.within(50, 100, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be within 50..100");

    expect(() => {
      expect("foo").to.have.length.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length within 5..7"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.within(5, 7);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length within 5..7"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length within 5..7"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length within 5..7"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length within 5..7"
    );

    expect(() => {
      expect(null).to.be.within(0, 1, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.within(0, 1);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.within(null, 1, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1, "blah").to.be.within(null, 1);
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1).to.be.within(0, null, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1, "blah").to.be.within(0, null);
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(null).to.not.be.within(0, 1, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.within(null, 1, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1).to.not.be.within(0, null, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(1).to.have.length.within(5, 7, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.within(5, 7);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.within(5, 7, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("within(start, finish) (dates)", () => {
    const now = new Date();
    const oneSecondAgo = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);
    const nowISO = now.toISOString();
    const beforeISO = oneSecondAgo.toISOString();
    const afterISO = oneSecondAfter.toISOString();

    expect(now).to.be.within(oneSecondAgo, oneSecondAfter);
    expect(now).to.be.within(now, oneSecondAfter);
    expect(now).to.be.within(now, now);
    expect(oneSecondAgo).to.not.be.within(now, oneSecondAfter);

    expect(() => {
      expect(now).to.not.be.within(now, oneSecondAfter, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        nowISO +
        " to not be within " +
        nowISO +
        ".." +
        afterISO
    );

    expect(() => {
      expect(now, "blah").to.not.be.within(oneSecondAgo, oneSecondAfter);
    }).to.throw(
      AssertionError,
      "blah: expected " +
        nowISO +
        " to not be within " +
        beforeISO +
        ".." +
        afterISO
    );

    expect(() => {
      expect(now).to.have.length.within(5, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to have property 'length'"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.within(now, 7, "blah");
    }).to.throw(
      AssertionError,
      "blah: the arguments to within must be numbers"
    );

    expect(() => {
      expect(now).to.be.within(now, 1, "blah");
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now).to.be.within(null, now, "blah");
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now).to.be.within(now, undefined, "blah");
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now, "blah").to.be.within(1, now);
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(now, "blah").to.be.within(now, 1);
    }).to.throw(AssertionError, "blah: the arguments to within must be dates");

    expect(() => {
      expect(null).to.not.be.within(now, oneSecondAfter, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");
  });

  it("above(n)", () => {
    expect(5).to.be.above(2);
    expect(5).to.be.greaterThan(2);
    expect(5).to.not.be.above(5);
    expect(5).to.not.be.above(6);
    expect("foo").to.have.length.above(2);
    expect("foo").to.have.lengthOf.above(2);
    expect([1, 2, 3]).to.have.length.above(2);
    expect([1, 2, 3]).to.have.lengthOf.above(2);

    expect(() => {
      expect(5).to.be.above(6, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to be above 6");

    expect(() => {
      expect(5, "blah").to.be.above(6);
    }).to.throw(AssertionError, "blah: expected 5 to be above 6");

    expect(() => {
      expect(10).to.not.be.above(6, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be at most 6");

    expect(() => {
      expect("foo").to.have.length.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length above 4 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.above(4);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length above 4 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length above 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length above 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length above 4 but got 3"
    );

    expect(() => {
      expect(null).to.be.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.above(0);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.above(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(1, "blah").to.be.above(null);
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(null).to.not.be.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.above(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(1).to.have.length.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.above(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.above(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("above(n) (dates)", () => {
    const now = new Date();
    const oneSecondAgo = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);

    expect(now).to.be.above(oneSecondAgo);
    expect(now).to.be.greaterThan(oneSecondAgo);
    expect(now).to.not.be.above(now);
    expect(now).to.not.be.above(oneSecondAfter);

    expect(() => {
      expect(now).to.be.above(oneSecondAfter, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        now.toISOString() +
        " to be above " +
        oneSecondAfter.toISOString()
    );

    expect(() => {
      expect(10).to.not.be.above(6, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be at most 6");

    expect(() => {
      expect(now).to.have.length.above(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + now.toISOString() + " to have property 'length'"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.above(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a number");

    expect(() => {
      expect(null).to.be.above(now, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(now).to.be.above(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to above must be a date");

    expect(() => {
      expect(null).to.have.length.above(0, "blah");
    }).to.throw(AssertionError, "blah: Target cannot be null or undefined.");
  });

  it("least(n)", () => {
    expect(5).to.be.at.least(2);
    expect(5).to.be.at.least(5);
    expect(5).to.not.be.at.least(6);
    expect("foo").to.have.length.of.at.least(2);
    expect("foo").to.have.lengthOf.at.least(2);
    expect([1, 2, 3]).to.have.length.of.at.least(2);
    expect([1, 2, 3]).to.have.lengthOf.at.least(2);

    expect(() => {
      expect(5).to.be.at.least(6, "blah");
    }).to.throw(AssertionError, "blah: expected 5 to be at least 6");

    expect(() => {
      expect(5, "blah").to.be.at.least(6);
    }).to.throw(AssertionError, "blah: expected 5 to be at least 6");

    expect(() => {
      expect(10).to.not.be.at.least(6, "blah");
    }).to.throw(AssertionError, "blah: expected 10 to be below 6");

    expect(() => {
      expect("foo").to.have.length.of.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at least 4 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.of.at.least(4);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at least 4 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at least 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.of.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at least 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at least 4 but got 3"
    );

    expect(() => {
      expect([1, 2, 3, 4]).to.not.have.length.of.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3, 4 ] to have a length below 4"
    );

    expect(() => {
      expect([1, 2, 3, 4]).to.not.have.lengthOf.at.least(4, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3, 4 ] to have a length below 4"
    );

    expect(() => {
      expect(null).to.be.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.at.least(0);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.at.least(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to least must be a number");

    expect(() => {
      expect(1, "blah").to.be.at.least(null);
    }).to.throw(AssertionError, "blah: the argument to least must be a number");

    expect(() => {
      expect(null).to.not.be.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.at.least(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to least must be a number");

    expect(() => {
      expect(1).to.have.length.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.at.least(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.at.least(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("below(n)", () => {
    expect(2).to.be.below(5);
    expect(2).to.be.lessThan(5);
    expect(2).to.not.be.below(2);
    expect(2).to.not.be.below(1);
    expect("foo").to.have.length.below(4);
    expect("foo").to.have.lengthOf.below(4);
    expect([1, 2, 3]).to.have.length.below(4);
    expect([1, 2, 3]).to.have.lengthOf.below(4);

    expect(() => {
      expect(6).to.be.below(5, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be below 5");

    expect(() => {
      expect(6, "blah").to.be.below(5);
    }).to.throw(AssertionError, "blah: expected 6 to be below 5");

    expect(() => {
      expect(6).to.not.be.below(10, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be at least 10");

    expect(() => {
      expect("foo").to.have.length.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.below(2);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length below 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length below 2 but got 3"
    );

    expect(() => {
      expect(null).to.be.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.below(0);
    }, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(1, "blah").to.be.below(null);
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(null).to.not.be.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(1).to.have.length.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.below(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.below(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("below(n) (dates)", () => {
    const now = new Date();
    const oneSecondAgo = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);

    expect(now).to.be.below(oneSecondAfter);
    expect(oneSecondAgo).to.be.lessThan(now);
    expect(now).to.not.be.below(oneSecondAgo);
    expect(oneSecondAfter).to.not.be.below(oneSecondAgo);

    expect(() => {
      expect(now).to.be.below(oneSecondAgo, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        now.toISOString() +
        " to be below " +
        oneSecondAgo.toISOString()
    );

    expect(() => {
      expect(now).to.not.be.below(oneSecondAfter, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " +
        now.toISOString() +
        " to be at least " +
        oneSecondAfter.toISOString()
    );

    expect(() => {
      expect("foo").to.have.length.below(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length below 2 but got 3"
    );

    expect(() => {
      expect(null).to.be.below(now, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");

    expect(() => {
      expect(now).to.not.be.below(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a date");

    expect(() => {
      expect(now).to.have.length.below(0, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + now.toISOString() + " to have property 'length'"
    );

    expect(() => {
      expect("asdasd").to.have.length.below(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to below must be a number");
  });

  it("most(n)", () => {
    expect(2).to.be.at.most(5);
    expect(2).to.be.at.most(2);
    expect(2).to.not.be.at.most(1);
    expect("foo").to.have.length.of.at.most(4);
    expect("foo").to.have.lengthOf.at.most(4);
    expect([1, 2, 3]).to.have.length.of.at.most(4);
    expect([1, 2, 3]).to.have.lengthOf.at.most(4);

    expect(() => {
      expect(6).to.be.at.most(5, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be at most 5");

    expect(() => {
      expect(6, "blah").to.be.at.most(5);
    }).to.throw(AssertionError, "blah: expected 6 to be at most 5");

    expect(() => {
      expect(6).to.not.be.at.most(10, "blah");
    }).to.throw(AssertionError, "blah: expected 6 to be above 10");

    expect(() => {
      expect("foo").to.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at most 2 but got 3"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.of.at.most(2);
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at most 2 but got 3"
    );

    expect(() => {
      expect("foo").to.have.lengthOf.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'foo' to have a length at most 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at most 2 but got 3"
    );

    expect(() => {
      expect([1, 2, 3]).to.have.lengthOf.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have a length at most 2 but got 3"
    );

    expect(() => {
      expect([1, 2]).to.not.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2 ] to have a length above 2"
    );

    expect(() => {
      expect([1, 2]).to.not.have.lengthOf.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2 ] to have a length above 2"
    );

    expect(() => {
      expect(null).to.be.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(null, "blah").to.be.at.most(0);
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.be.at.most(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(1, "blah").to.be.at.most(null);
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(null).to.not.be.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(1).to.not.be.at.most(null, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(1).to.have.length.of.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1, "blah").to.have.length.of.at.most(0);
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");

    expect(() => {
      expect(1).to.have.lengthOf.at.most(0, "blah");
    }).to.throw(AssertionError, "blah: expected 1 to have property 'length'");
  });

  it("most(n) (dates)", () => {
    const now = new Date();
    const oneSecondBefore = new Date(now.getTime() - 1000);
    const oneSecondAfter = new Date(now.getTime() + 1000);
    const nowISO = now.toISOString();
    const beforeISO = oneSecondBefore.toISOString();

    expect(now).to.be.at.most(oneSecondAfter);
    expect(now).to.be.at.most(now);
    expect(now).to.not.be.at.most(oneSecondBefore);

    expect(() => {
      expect(now).to.be.at.most(oneSecondBefore, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to be at most " + beforeISO
    );

    expect(() => {
      expect(now).to.not.be.at.most(now, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to be above " + nowISO
    );

    expect(() => {
      expect(now).to.have.length.of.at.most(2, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected " + nowISO + " to have property 'length'"
    );

    expect(() => {
      expect("foo", "blah").to.have.length.of.at.most(now);
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect([1, 2, 3]).to.not.have.length.of.at.most(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(null).to.be.at.most(now, "blah");
    }).to.throw(AssertionError, "blah: expected null to be a number or a date");

    expect(() => {
      expect(now, "blah").to.be.at.most(null);
    }).to.throw(AssertionError, "blah: the argument to most must be a date");

    expect(() => {
      expect(1).to.be.at.most(now, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a number");

    expect(() => {
      expect(now, "blah").to.be.at.most(1);
    }).to.throw(AssertionError, "blah: the argument to most must be a date");

    expect(() => {
      expect(now).to.not.be.at.most(undefined, "blah");
    }).to.throw(AssertionError, "blah: the argument to most must be a date");
  });

  it("match(regexp)", () => {
    expect("foobar").to.match(/^foo/);
    expect("foobar").to.matches(/^foo/);
    expect("foobar").to.not.match(/^bar/);

    expect(() => {
      expect("foobar").to.match(/^bar/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      expect("foobar", "blah").to.match(/^bar/i);
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      expect("foobar").to.matches(/^bar/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to match /^bar/i");

    expect(() => {
      expect("foobar").to.not.match(/^foo/i, "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' not to match /^foo/i");
  });

  it("lengthOf(n)", () => {
    expect("test").to.have.length(4);
    expect("test").to.have.lengthOf(4);
    expect("test").to.not.have.length(3);
    expect("test").to.not.have.lengthOf(3);
    expect([1, 2, 3]).to.have.length(3);
    expect([1, 2, 3]).to.have.lengthOf(3);

    expect(() => {
      expect(4).to.have.length(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to have property 'length'");

    expect(() => {
      expect(4, "blah").to.have.length(3);
    }).to.throw(AssertionError, "blah: expected 4 to have property 'length'");

    expect(() => {
      expect(4).to.have.lengthOf(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to have property 'length'");

    expect(() => {
      expect("asd").to.not.have.length(3, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'asd' to not have a length of 3"
    );
    expect(() => {
      expect("asd").to.not.have.lengthOf(3, "blah");
    }).to.throw(
      AssertionError,
      "blah: expected 'asd' to not have a length of 3"
    );
  });

  it("eql(val)", () => {
    expect("test").to.eql("test");
    expect({ foo: "bar" }).to.eql({ foo: "bar" });
    expect(1).to.eql(1);
    expect("4").to.not.eql(4);
    expect(sym).to.eql(sym);

    expect(() => {
      expect(4).to.eql(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to deeply equal 3");
  });

  it("equal(val)", () => {
    expect("test").to.equal("test");
    expect(1).to.equal(1);
    expect(sym).to.equal(sym);

    expect(() => {
      expect(4).to.equal(3, "blah");
    }).to.throw(AssertionError, "blah: expected 4 to equal 3");

    expect(() => {
      expect(4, "blah").to.equal(3);
    }).to.throw(AssertionError, "blah: expected 4 to equal 3");

    expect(() => {
      expect("4").to.equal(4, "blah");
    }).to.throw(AssertionError, "blah: expected '4' to equal 4");
  });

  it("deep.equal(val)", () => {
    expect({ foo: "bar" }).to.deep.equal({ foo: "bar" });
    expect({ foo: "bar" }).not.to.deep.equal({ foo: "baz" });
  });

  it("deep.equal(/regexp/)", () => {
    expect(/a/).to.deep.equal(/a/);
    expect(/a/).not.to.deep.equal(/b/);
    expect(/a/).not.to.deep.equal({});
    expect(/a/g).to.deep.equal(/a/g);
    expect(/a/g).not.to.deep.equal(/b/g);
    expect(/a/i).to.deep.equal(/a/i);
    expect(/a/i).not.to.deep.equal(/b/i);
    expect(/a/m).to.deep.equal(/a/m);
    expect(/a/m).not.to.deep.equal(/b/m);
  });

  it("deep.equal(Date)", () => {
    var a = new Date(1, 2, 3),
      b = new Date(4, 5, 6);
    expect(a).to.deep.equal(a);
    expect(a).not.to.deep.equal(b);
    expect(a).not.to.deep.equal({});
  });

  it("empty", () => {
    function FakeArgs() {}
    FakeArgs.prototype.length = 0;

    expect("").to.be.empty;
    expect("foo").not.to.be.empty;
    expect([]).to.be.empty;
    expect(["foo"]).not.to.be.empty;
    expect(new FakeArgs()).to.be.empty;
    expect({ arguments: 0 }).not.to.be.empty;
    expect({}).to.be.empty;
    expect({ foo: "bar" }).not.to.be.empty;

    expect(() => {
      expect(new WeakMap(), "blah").not.to.be.empty;
    }).to.throw(AssertionError, "blah: .empty was passed a weak collection");

    expect(() => {
      expect(new WeakSet(), "blah").not.to.be.empty;
    }).to.throw(AssertionError, "blah: .empty was passed a weak collection");

    expect(new Map()).to.be.empty;

    // Not using Map constructor args because not supported in IE 11.
    var map = new Map();
    map.set("a", 1);
    expect(map).not.to.be.empty;

    expect(() => {
      expect(new Map()).not.to.be.empty;
    }).to.throw(AssertionError, "expected Map{} not to be empty");

    map = new Map();
    map.key = "val";
    expect(map).to.be.empty;

    expect(() => {
      expect(map).not.to.be.empty;
    }).to.throw(AssertionError, "expected Map{} not to be empty");

    expect(new Set()).to.be.empty;

    // Not using Set constructor args because not supported in IE 11.
    var set = new Set();
    set.add(1);
    expect(set).not.to.be.empty;

    expect(() => {
      expect(new Set()).not.to.be.empty;
    }).to.throw(AssertionError, "expected Set{} not to be empty");

    set = new Set();
    set.key = "val";
    expect(set).to.be.empty;

    expect(() => {
      expect(set).not.to.be.empty;
    }).to.throw(AssertionError, "expected Set{} not to be empty");

    expect(() => {
      expect("", "blah").not.to.be.empty;
    }).to.throw(AssertionError, "blah: expected '' not to be empty");

    expect(() => {
      expect("foo").to.be.empty;
    }).to.throw(AssertionError, "expected 'foo' to be empty");

    expect(() => {
      expect([]).not.to.be.empty;
    }).to.throw(AssertionError, "expected [] not to be empty");

    expect(() => {
      expect(["foo"]).to.be.empty;
    }).to.throw(AssertionError, "expected [ 'foo' ] to be empty");

    expect(() => {
      expect(new FakeArgs()).not.to.be.empty;
    }).to.throw(AssertionError, "expected FakeArgs{} not to be empty");

    expect(() => {
      expect({ arguments: 0 }).to.be.empty;
    }).to.throw(AssertionError, "expected { arguments: +0 } to be empty");

    expect(() => {
      expect({}).not.to.be.empty;
    }).to.throw(AssertionError, "expected {} not to be empty");

    expect(() => {
      expect({ foo: "bar" }).to.be.empty;
    }).to.throw(AssertionError, "expected { foo: 'bar' } to be empty");

    expect(() => {
      expect(null, "blah").to.be.empty;
    }).to.throw(
      AssertionError,
      "blah: .empty was passed non-string primitive null"
    );

    expect(() => {
      expect(undefined).to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect().to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect(null).to.not.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive null");

    expect(() => {
      expect(undefined).to.not.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect().to.not.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive undefined"
    );

    expect(() => {
      expect(0).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive +0");

    expect(() => {
      expect(1).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive 1");

    expect(() => {
      expect(true).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive true");

    expect(() => {
      expect(false).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed non-string primitive false");

    expect(() => {
      expect(Symbol()).to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive Symbol()"
    );

    expect(() => {
      expect(Symbol.iterator).to.be.empty;
    }).to.throw(
      AssertionError,
      ".empty was passed non-string primitive Symbol(Symbol.iterator)"
    );

    expect(() => {
      expect(function () {}, "blah").to.be.empty;
    }).to.throw(AssertionError, "blah: .empty was passed a function");

    expect(() => {
      expect(FakeArgs).to.be.empty;
    }).to.throw(AssertionError, ".empty was passed a function FakeArgs");
  });

  it("string()", () => {
    expect("foobar").to.have.string("bar");
    expect("foobar").to.have.string("foo");
    expect("foobar").to.not.have.string("baz");

    expect(() => {
      expect(3).to.have.string("baz", "blah");
    }).to.throw(AssertionError, "blah: expected 3 to be a string");

    expect(() => {
      expect(3, "blah").to.have.string("baz");
    }).to.throw(AssertionError, "blah: expected 3 to be a string");

    expect(() => {
      expect("foobar").to.have.string("baz", "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to contain 'baz'");

    expect(() => {
      expect("foobar", "blah").to.have.string("baz");
    }).to.throw(AssertionError, "blah: expected 'foobar' to contain 'baz'");

    expect(() => {
      expect("foobar").to.not.have.string("bar", "blah");
    }).to.throw(AssertionError, "blah: expected 'foobar' to not contain 'bar'");
  });

  it("NaN", () => {
    expect(NaN).to.be.NaN;
    expect(undefined).not.to.be.NaN;
    expect(Infinity).not.to.be.NaN;
    expect("foo").not.to.be.NaN;
    expect({}).not.to.be.NaN;
    expect(4).not.to.be.NaN;
    expect([]).not.to.be.NaN;

    expect(() => {
      expect(NaN, "blah").not.to.be.NaN;
    }).to.throw(AssertionError, "blah: expected NaN not to be NaN");

    expect(() => {
      expect(undefined).to.be.NaN;
    }).to.throw(AssertionError, "expected undefined to be NaN");

    expect(() => {
      expect(Infinity).to.be.NaN;
    }).to.throw(AssertionError, "expected Infinity to be NaN");

    expect(() => {
      expect("foo").to.be.NaN;
    }).to.throw(AssertionError, "expected 'foo' to be NaN");

    expect(() => {
      expect({}).to.be.NaN;
    }).to.throw(AssertionError, "expected {} to be NaN");

    expect(() => {
      expect(4).to.be.NaN;
    }).to.throw(AssertionError, "expected 4 to be NaN");

    expect(() => {
      expect([]).to.be.NaN;
    }).to.throw(AssertionError, "expected [] to be NaN");
  });

  it("finite", function () {
    expect(4).to.be.finite;
    expect(-10).to.be.finite;

    expect(() => {
      expect(NaN, "blah").to.be.finite;
    }).to.throw(AssertionError, "blah: expected NaN to be a finite number");

    expect(() => {
      expect(Infinity).to.be.finite;
    }).to.throw(AssertionError, "expected Infinity to be a finite number");

    expect(() => {
      expect("foo").to.be.finite;
    }).to.throw(AssertionError, "expected 'foo' to be a finite number");

    expect(() => {
      expect([]).to.be.finite;
    }).to.throw(AssertionError, "expected [] to be a finite number");

    expect(() => {
      expect({}).to.be.finite;
    }).to.throw(AssertionError, "expected {} to be a finite number");
  });

  it("property(name)", function () {
    expect("test").to.have.property("length");
    expect({ a: 1 }).to.have.property("toString");
    expect(4).to.not.have.property("length");

    expect({ "foo.bar": "baz" }).to.have.property("foo.bar");
    expect({ foo: { bar: "baz" } }).to.not.have.property("foo.bar");

    // Properties with the value 'undefined' are still properties
    var obj = { foo: undefined };
    Object.defineProperty(obj, "bar", {
      get: function () {},
    });
    expect(obj).to.have.property("foo");
    expect(obj).to.have.property("bar");

    expect({ "foo.bar[]": "baz" }).to.have.property("foo.bar[]");

    expect(() => {
      expect("asd").to.have.property("foo");
    }).to.throw(AssertionError, "expected 'asd' to have property 'foo'");

    expect(() => {
      expect("asd", "blah").to.have.property("foo");
    }).to.throw(AssertionError, "blah: expected 'asd' to have property 'foo'");

    expect(() => {
      expect({ foo: { bar: "baz" } }).to.have.property("foo.bar");
    }).to.throw(
      AssertionError,
      "expected { foo: { bar: 'baz' } } to have property 'foo.bar'"
    );

    expect(() => {
      expect({ a: { b: 1 } }).to.have.own.nested.property("a.b");
    }).to.throw(
      AssertionError,
      'The "nested" and "own" flags cannot be combined.'
    );

    expect(() => {
      expect({ a: { b: 1 } }, "blah").to.have.own.nested.property("a.b");
    }).to.throw(
      AssertionError,
      'blah: The "nested" and "own" flags cannot be combined.'
    );

    expect(() => {
      expect(null, "blah").to.have.property("a");
    }).to.throw(AssertionError, "blah: Target cannot be null or undefined.");

    expect(() => {
      expect(undefined, "blah").to.have.property("a");
    }).to.throw(AssertionError, "blah: Target cannot be null or undefined.");
  });

  it("include()", () => {
    expect(["foo", "bar"]).to.include("foo");
    expect(["foo", "bar"]).to.include("foo");
    expect(["foo", "bar"]).to.include("bar");
    expect([1, 2]).to.include(1);
    expect(["foo", "bar"]).to.not.include("baz");
    expect(["foo", "bar"]).to.not.include(1);

    expect({ a: 1 }).to.include({ toString: Object.prototype.toString });

    // .include should work with Error objects and objects with a custom
    // `@@toStringTag`.
    expect(new Error("foo")).to.include({ message: "foo" });
    var customObj = { a: 1 };
    customObj[Symbol.toStringTag] = "foo";

    expect(customObj).to.include({ a: 1 });

    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    expect([obj1, obj2]).to.include(obj1);
    expect([obj1, obj2]).to.not.include({ a: 1 });
    expect({ foo: obj1, bar: obj2 }).to.include({ foo: obj1 });
    expect({ foo: obj1, bar: obj2 }).to.include({ foo: obj1, bar: obj2 });
    expect({ foo: obj1, bar: obj2 }).to.not.include({ foo: { a: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.not.include({
      foo: obj1,
      bar: { b: 2 },
    });

    var map = new Map();
    var val = [{ a: 1 }];
    map.set("a", val);
    map.set("b", 2);
    map.set("c", -0);
    map.set("d", NaN);

    expect(map).to.include(val);
    expect(map).to.not.include([{ a: 1 }]);
    expect(map).to.include(2);
    expect(map).to.not.include(3);
    expect(map).to.include(0);
    expect(map).to.include(NaN);

    var set = new Set();
    var val = [{ a: 1 }];
    set.add(val);
    set.add(2);
    set.add(-0);
    set.add(NaN);

    expect(set).to.include(val);
    expect(set).to.not.include([{ a: 1 }]);
    expect(set).to.include(2);
    expect(set).to.not.include(3);
    expect(set).to.include(NaN);

    var ws = new WeakSet();
    var val = [{ a: 1 }];
    ws.add(val);

    expect(ws).to.include(val);
    expect(ws).to.not.include([{ a: 1 }]);
    expect(ws).to.not.include({});

    var sym1 = Symbol(),
      sym2 = Symbol(),
      sym3 = Symbol();
    expect([sym1, sym2]).to.include(sym1);
    expect([sym1, sym2]).to.not.include(sym3);
  });

  it("deep.include()", () => {
    var obj1 = { a: 1 },
      obj2 = { b: 2 };
    expect([obj1, obj2]).to.deep.include({ a: 1 });
    expect([obj1, obj2]).to.not.deep.include({ a: 9 });
    expect([obj1, obj2]).to.not.deep.include({ z: 1 });
    expect({ foo: obj1, bar: obj2 }).to.deep.include({ foo: { a: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.deep.include({
      foo: { a: 1 },
      bar: { b: 2 },
    });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({ foo: { a: 9 } });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({ foo: { z: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({ baz: { a: 1 } });
    expect({ foo: obj1, bar: obj2 }).to.not.deep.include({
      foo: { a: 1 },
      bar: { b: 9 },
    });

    var map = new Map();
    map.set(1, [{ a: 1 }]);

    expect(map).to.deep.include([{ a: 1 }]);

    var set = new Set();
    set.add([{ a: 1 }]);

    expect(set).to.deep.include([{ a: 1 }]);
  });

  it("nested.include()", () => {
    expect({ a: { b: ["x", "y"] } }).to.nested.include({ "a.b[1]": "y" });
    expect({ a: { b: ["x", "y"] } }).to.not.nested.include({ "a.b[1]": "x" });
    expect({ a: { b: ["x", "y"] } }).to.not.nested.include({ "a.c": "y" });

    expect({ a: { b: [{ x: 1 }] } }).to.not.nested.include({
      "a.b[0]": { x: 1 },
    });

    expect({ ".a": { "[b]": "x" } }).to.nested.include({ "\\.a.\\[b\\]": "x" });
    expect({ ".a": { "[b]": "x" } }).to.not.nested.include({
      "\\.a.\\[b\\]": "y",
    });
  });

  it("deep.nested.include()", () => {
    expect({ a: { b: [{ x: 1 }] } }).to.deep.nested.include({
      "a.b[0]": { x: 1 },
    });
    expect({ a: { b: [{ x: 1 }] } }).to.not.deep.nested.include({
      "a.b[0]": { y: 2 },
    });
    expect({ a: { b: [{ x: 1 }] } }).to.not.deep.nested.include({
      "a.c": { x: 1 },
    });

    expect({ ".a": { "[b]": { x: 1 } } }).to.deep.nested.include({
      "\\.a.\\[b\\]": { x: 1 },
    });
    expect({ ".a": { "[b]": { x: 1 } } }).to.not.deep.nested.include({
      "\\.a.\\[b\\]": { y: 2 },
    });
  });

  it("own.include()", () => {
    expect({ a: 1 }).to.own.include({ a: 1 });
    expect({ a: 1 }).to.not.own.include({ a: 3 });
    expect({ a: 1 }).to.not.own.include({
      toString: Object.prototype.toString,
    });

    expect({ a: { b: 2 } }).to.not.own.include({ a: { b: 2 } });
  });

  it("deep.own.include()", () => {
    expect({ a: { b: 2 } }).to.deep.own.include({ a: { b: 2 } });
    expect({ a: { b: 2 } }).to.not.deep.own.include({ a: { c: 3 } });
    expect({ a: { b: 2 } }).to.not.deep.own.include({
      toString: Object.prototype.toString,
    });
  });

  it("keys(array|Object|arguments)", () => {
    expect({ foo: 1 }).to.have.keys(["foo"]);
    expect({ foo: 1 }).have.keys({ foo: 6 });
    expect({ foo: 1, bar: 2 }).to.have.keys(["foo", "bar"]);
    expect({ foo: 1, bar: 2 }).to.have.keys("foo", "bar");
    expect({ foo: 1, bar: 2 }).have.keys({ foo: 6, bar: 7 });
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.keys("foo", "bar");
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.keys("bar", "foo");
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.keys("baz");
    expect({ foo: 1, bar: 2 }).contain.keys({ foo: 6 });
    expect({ foo: 1, bar: 2 }).contain.keys({ bar: 7 });
    expect({ foo: 1, bar: 2 }).contain.keys({ foo: 6 });

    expect({ foo: 1, bar: 2 }).to.contain.keys("foo");
    expect({ foo: 1, bar: 2 }).to.contain.keys("bar", "foo");
    expect({ foo: 1, bar: 2 }).to.contain.keys(["foo"]);
    expect({ foo: 1, bar: 2 }).to.contain.keys(["bar"]);
    expect({ foo: 1, bar: 2 }).to.contain.keys(["bar", "foo"]);
    expect({ foo: 1, bar: 2, baz: 3 }).to.contain.all.keys(["bar", "foo"]);

    expect({ foo: 1, bar: 2 }).to.not.have.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.have.keys("foo");
    expect({ foo: 1, bar: 2 }).to.not.have.keys("foo", "baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.keys("foo", "baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.keys("baz", "foo");

    expect({ foo: 1, bar: 2 }).to.have.any.keys("foo", "baz");
    expect({ foo: 1, bar: 2 }).to.have.any.keys("foo");
    expect({ foo: 1, bar: 2 }).to.contain.any.keys("bar", "baz");
    expect({ foo: 1, bar: 2 }).to.contain.any.keys(["foo"]);
    expect({ foo: 1, bar: 2 }).to.have.all.keys(["bar", "foo"]);
    expect({ foo: 1, bar: 2 }).to.contain.all.keys(["bar", "foo"]);
    expect({ foo: 1, bar: 2 }).contain.any.keys({ foo: 6 });
    expect({ foo: 1, bar: 2 }).have.all.keys({ foo: 6, bar: 7 });
    expect({ foo: 1, bar: 2 }).contain.all.keys({ bar: 7, foo: 6 });

    expect({ foo: 1, bar: 2 }).to.not.have.any.keys("baz", "abc", "def");
    expect({ foo: 1, bar: 2 }).to.not.have.any.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.contain.any.keys("baz");
    expect({ foo: 1, bar: 2 }).to.not.have.all.keys(["baz", "foo"]);
    expect({ foo: 1, bar: 2 }).to.not.contain.all.keys(["baz", "foo"]);
    expect({ foo: 1, bar: 2 }).not.have.all.keys({ baz: 8, foo: 7 });
    expect({ foo: 1, bar: 2 }).not.contain.all.keys({ baz: 8, foo: 7 });

    var enumProp1 = "enumProp1",
      enumProp2 = "enumProp2",
      nonEnumProp = "nonEnumProp",
      obj = {};

    obj[enumProp1] = "enumProp1";
    obj[enumProp2] = "enumProp2";

    Object.defineProperty(obj, nonEnumProp, {
      enumerable: false,
      value: "nonEnumProp",
    });

    expect(obj).to.have.all.keys([enumProp1, enumProp2]);
    expect(obj).to.not.have.all.keys([enumProp1, enumProp2, nonEnumProp]);

    var sym1 = Symbol("sym1"),
      sym2 = Symbol("sym2"),
      sym3 = Symbol("sym3"),
      str = "str",
      obj = {};

    obj[sym1] = "sym1";
    obj[sym2] = "sym2";
    obj[str] = "str";

    Object.defineProperty(obj, sym3, {
      enumerable: false,
      value: "sym3",
    });

    expect(obj).to.have.all.keys([sym1, sym2, str]);
    expect(obj).to.not.have.all.keys([sym1, sym2, sym3, str]);

    // Not using Map constructor args because not supported in IE 11.
    var aKey = { thisIs: "anExampleObject" },
      anotherKey = { doingThisBecauseOf: "referential equality" },
      testMap = new Map();

    testMap.set(aKey, "aValue");
    testMap.set(anotherKey, "anotherValue");

    expect(testMap).to.have.any.keys(aKey);
    expect(testMap).to.have.any.keys("thisDoesNotExist", "thisToo", aKey);
    expect(testMap).to.have.all.keys(aKey, anotherKey);

    expect(testMap).to.contain.all.keys(aKey);
    expect(testMap).to.not.contain.all.keys(aKey, "thisDoesNotExist");

    expect(testMap).to.not.have.any.keys({ iDoNot: "exist" });
    expect(testMap).to.not.have.any.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testMap).to.not.have.all.keys(
      "thisDoesNotExist",
      "thisToo",
      anotherKey
    );

    expect(testMap).to.have.any.keys([aKey]);
    expect(testMap).to.have.any.keys([20, 1, aKey]);
    expect(testMap).to.have.all.keys([aKey, anotherKey]);

    expect(testMap).to.not.have.any.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testMap).to.not.have.any.keys([20, 1, { 13: 37 }]);
    expect(testMap).to.not.have.all.keys([aKey, { iDoNot: "exist" }]);

    // Using the same assertions as above but with `.deep` flag instead of using referential equality
    expect(testMap).to.have.any.deep.keys({ thisIs: "anExampleObject" });
    expect(testMap).to.have.any.deep.keys("thisDoesNotExist", "thisToo", {
      thisIs: "anExampleObject",
    });

    expect(testMap).to.contain.all.deep.keys({ thisIs: "anExampleObject" });
    expect(testMap).to.not.contain.all.deep.keys(
      { thisIs: "anExampleObject" },
      "thisDoesNotExist"
    );

    expect(testMap).to.not.have.any.deep.keys({ iDoNot: "exist" });
    expect(testMap).to.not.have.any.deep.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testMap).to.not.have.all.deep.keys("thisDoesNotExist", "thisToo", {
      doingThisBecauseOf: "referential equality",
    });

    expect(testMap).to.have.any.deep.keys([{ thisIs: "anExampleObject" }]);
    expect(testMap).to.have.any.deep.keys([
      20,
      1,
      { thisIs: "anExampleObject" },
    ]);

    expect(testMap).to.have.all.deep.keys(
      { thisIs: "anExampleObject" },
      { doingThisBecauseOf: "referential equality" }
    );

    expect(testMap).to.not.have.any.deep.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testMap).to.not.have.any.deep.keys([20, 1, { 13: 37 }]);
    expect(testMap).to.not.have.all.deep.keys([
      { thisIs: "anExampleObject" },
      { iDoNot: "exist" },
    ]);

    var weirdMapKey1 = Object.create(null),
      weirdMapKey2 = { toString: NaN },
      weirdMapKey3 = [],
      weirdMap = new Map();

    weirdMap.set(weirdMapKey1, "val1");
    weirdMap.set(weirdMapKey2, "val2");

    expect(weirdMap).to.have.all.keys([weirdMapKey1, weirdMapKey2]);
    expect(weirdMap).to.not.have.all.keys([weirdMapKey1, weirdMapKey3]);

    var symMapKey1 = Symbol(),
      symMapKey2 = Symbol(),
      symMapKey3 = Symbol(),
      symMap = new Map();

    symMap.set(symMapKey1, "val1");
    symMap.set(symMapKey2, "val2");

    expect(symMap).to.have.all.keys(symMapKey1, symMapKey2);
    expect(symMap).to.have.any.keys(symMapKey1, symMapKey3);
    expect(symMap).to.contain.all.keys(symMapKey2, symMapKey1);
    expect(symMap).to.contain.any.keys(symMapKey3, symMapKey1);

    expect(symMap).to.not.have.all.keys(symMapKey1, symMapKey3);
    expect(symMap).to.not.have.any.keys(symMapKey3);
    expect(symMap).to.not.contain.all.keys(symMapKey3, symMapKey1);
    expect(symMap).to.not.contain.any.keys(symMapKey3);

    var aKey = { thisIs: "anExampleObject" },
      anotherKey = { doingThisBecauseOf: "referential equality" },
      testSet = new Set();

    testSet.add(aKey);
    testSet.add(anotherKey);

    expect(testSet).to.have.any.keys(aKey);
    expect(testSet).to.have.any.keys("thisDoesNotExist", "thisToo", aKey);
    expect(testSet).to.have.all.keys(aKey, anotherKey);

    expect(testSet).to.contain.all.keys(aKey);
    expect(testSet).to.not.contain.all.keys(aKey, "thisDoesNotExist");

    expect(testSet).to.not.have.any.keys({ iDoNot: "exist" });
    expect(testSet).to.not.have.any.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testSet).to.not.have.all.keys(
      "thisDoesNotExist",
      "thisToo",
      anotherKey
    );

    expect(testSet).to.have.any.keys([aKey]);
    expect(testSet).to.have.any.keys([20, 1, aKey]);
    expect(testSet).to.have.all.keys([aKey, anotherKey]);

    expect(testSet).to.not.have.any.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testSet).to.not.have.any.keys([20, 1, { 13: 37 }]);
    expect(testSet).to.not.have.all.keys([aKey, { iDoNot: "exist" }]);

    // Using the same assertions as above but with `.deep` flag instead of using referential equality
    expect(testSet).to.have.any.deep.keys({ thisIs: "anExampleObject" });
    expect(testSet).to.have.any.deep.keys("thisDoesNotExist", "thisToo", {
      thisIs: "anExampleObject",
    });

    expect(testSet).to.contain.all.deep.keys({ thisIs: "anExampleObject" });
    expect(testSet).to.not.contain.all.deep.keys(
      { thisIs: "anExampleObject" },
      "thisDoesNotExist"
    );

    expect(testSet).to.not.have.any.deep.keys({ iDoNot: "exist" });
    expect(testSet).to.not.have.any.deep.keys(
      "thisIsNotAkey",
      { iDoNot: "exist" },
      { 33: 20 }
    );
    expect(testSet).to.not.have.all.deep.keys("thisDoesNotExist", "thisToo", {
      doingThisBecauseOf: "referential equality",
    });

    expect(testSet).to.have.any.deep.keys([{ thisIs: "anExampleObject" }]);
    expect(testSet).to.have.any.deep.keys([
      20,
      1,
      { thisIs: "anExampleObject" },
    ]);

    expect(testSet).to.have.all.deep.keys([
      { thisIs: "anExampleObject" },
      { doingThisBecauseOf: "referential equality" },
    ]);

    expect(testSet).to.not.have.any.deep.keys([
      { 13: 37 },
      "thisDoesNotExist",
      "thisToo",
    ]);
    expect(testSet).to.not.have.any.deep.keys([20, 1, { 13: 37 }]);
    expect(testSet).to.not.have.all.deep.keys([
      { thisIs: "anExampleObject" },
      { iDoNot: "exist" },
    ]);

    var weirdSetKey1 = Object.create(null),
      weirdSetKey2 = { toString: NaN },
      weirdSetKey3 = [],
      weirdSet = new Set();

    weirdSet.add(weirdSetKey1);
    weirdSet.add(weirdSetKey2);

    expect(weirdSet).to.have.all.keys([weirdSetKey1, weirdSetKey2]);
    expect(weirdSet).to.not.have.all.keys([weirdSetKey1, weirdSetKey3]);

    var symSetKey1 = Symbol(),
      symSetKey2 = Symbol(),
      symSetKey3 = Symbol(),
      symSet = new Set();

    symSet.add(symSetKey1);
    symSet.add(symSetKey2);

    expect(symSet).to.have.all.keys(symSetKey1, symSetKey2);
    expect(symSet).to.have.any.keys(symSetKey1, symSetKey3);
    expect(symSet).to.contain.all.keys(symSetKey2, symSetKey1);
    expect(symSet).to.contain.any.keys(symSetKey3, symSetKey1);

    expect(symSet).to.not.have.all.keys(symSetKey1, symSetKey3);
    expect(symSet).to.not.have.any.keys(symSetKey3);
    expect(symSet).to.not.contain.all.keys(symSetKey3, symSetKey1);
    expect(symSet).to.not.contain.any.keys(symSetKey3);
  });

  it("keys(array) will not mutate array (#359)", () => {
    var expected = ["b", "a"];
    var original_order = ["b", "a"];
    var obj = { b: 1, a: 1 };
    expect(expected).deep.equal(original_order);
    expect(obj).keys(original_order);
    expect(expected).deep.equal(original_order);
  });

  it("chaining", () => {
    var tea = { name: "chai", extras: ["milk", "sugar", "smile"] };
    expect(tea).to.have.property("extras").with.lengthOf(3);

    expect(tea).to.have.property("extras").which.contains("smile");

    expect(() => {
      expect(tea).to.have.property("extras").with.lengthOf(4);
    }).to.throw(
      AssertionError,
      "expected [ 'milk', 'sugar', 'smile' ] to have a length of 4 but got 3"
    );

    expect(tea).to.be.a("object").and.have.property("name", "chai");

    var badFn = function () {
      throw new Error("testing");
    };

    expect(badFn).to.throw(Error).with.property("message", "testing");
  });

  it("throw", function () {
    // See GH-45: some poorly-constructed custom errors don't have useful names
    // on either their constructor or their constructor prototype, but instead
    // only set the name inside the constructor itself.
    var PoorlyConstructedError = function () {
      this.name = "PoorlyConstructedError";
    };
    PoorlyConstructedError.prototype = Object.create(Error.prototype);

    function CustomError(message) {
      this.name = "CustomError";
      this.message = message;
    }
    CustomError.prototype = Error.prototype;

    var specificError = new RangeError("boo");

    var goodFn = function () {
        1 == 1;
      },
      badFn = function () {
        throw new Error("testing");
      },
      refErrFn = function () {
        throw new ReferenceError("hello");
      },
      ickyErrFn = function () {
        throw new PoorlyConstructedError();
      },
      specificErrFn = function () {
        throw specificError;
      },
      customErrFn = function () {
        throw new CustomError("foo");
      },
      emptyErrFn = function () {
        throw new Error();
      },
      emptyStringErrFn = function () {
        throw new Error("");
      };

    expect(goodFn).to.not.throw();
    expect(goodFn).to.not.throw(Error);
    expect(goodFn).to.not.throw(specificError);
    expect(badFn).to.throw();
    expect(badFn).to.throw(Error);
    expect(badFn).to.not.throw(ReferenceError);
    expect(badFn).to.not.throw(specificError);
    expect(refErrFn).to.throw();
    expect(refErrFn).to.throw(ReferenceError);
    expect(refErrFn).to.throw(Error);
    expect(refErrFn).to.not.throw(TypeError);
    expect(refErrFn).to.not.throw(specificError);
    expect(ickyErrFn).to.throw();
    expect(ickyErrFn).to.throw(PoorlyConstructedError);
    expect(ickyErrFn).to.throw(Error);
    expect(ickyErrFn).to.not.throw(specificError);
    expect(specificErrFn).to.throw(specificError);

    expect(goodFn).to.not.throw("testing");
    expect(goodFn).to.not.throw(/testing/);
    expect(badFn).to.throw(/testing/);
    expect(badFn).to.not.throw(/hello/);
    expect(badFn).to.throw("testing");
    expect(badFn).to.not.throw("hello");
    expect(emptyStringErrFn).to.throw("");
    expect(emptyStringErrFn).to.not.throw("testing");
    expect(badFn).to.throw("");

    expect(badFn).to.throw(Error, /testing/);
    expect(badFn).to.throw(Error, "testing");
    expect(emptyErrFn).to.not.throw(Error, "testing");

    expect(badFn).to.not.throw(Error, "I am the wrong error message");
    expect(badFn).to.not.throw(TypeError, "testing");

    expect(() => {
      expect(goodFn, "blah").to.throw();
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( goodFn)*\] to throw an error$/
    );

    expect(() => {
      expect(goodFn, "blah").to.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( goodFn)*\] to throw ReferenceError$/
    );

    expect(() => {
      expect(goodFn, "blah").to.throw(specificError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( goodFn)*\] to throw 'RangeError: boo'$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw();
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw an error but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw 'ReferenceError' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(specificError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw 'RangeError: boo' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw(Error);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw 'Error' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(refErrFn, "blah").to.not.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( refErrFn)*\] to not throw 'ReferenceError' but 'ReferenceError: hello' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(PoorlyConstructedError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw 'PoorlyConstructedError' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(ickyErrFn, "blah").to.not.throw(PoorlyConstructedError);
    }).to.throw(
      AssertionError,
      /^blah: (expected \[Function( ickyErrFn)*\] to not throw 'PoorlyConstructedError' but)(.*)(PoorlyConstructedError|\{ Object \()(.*)(was thrown)$/
    );

    expect(() => {
      expect(ickyErrFn, "blah").to.throw(ReferenceError);
    }).to.throw(
      AssertionError,
      /^blah: (expected \[Function( ickyErrFn)*\] to throw 'ReferenceError' but)(.*)(PoorlyConstructedError|\{ Object \()(.*)(was thrown)$/
    );

    expect(() => {
      expect(specificErrFn, "blah").to.throw(new ReferenceError("eek"));
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( specificErrFn)*\] to throw 'ReferenceError: eek' but 'RangeError: boo' was thrown$/
    );

    expect(() => {
      expect(specificErrFn, "blah").to.not.throw(specificError);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( specificErrFn)*\] to not throw 'RangeError: boo'$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw(/testing/);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error not matching \/testing\/$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(/hello/);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error matching \/hello\/ but got 'testing'$/
    );

    expect(() => {
      expect(badFn).to.throw(Error, /hello/, "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error matching \/hello\/ but got 'testing'$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(Error, /hello/);
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error matching \/hello\/ but got 'testing'$/
    );

    expect(() => {
      expect(badFn).to.throw(Error, "hello", "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error including 'hello' but got 'testing'$/
    );

    expect(() => {
      expect(badFn, "blah").to.throw(Error, "hello");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to throw error including 'hello' but got 'testing'$/
    );

    expect(() => {
      expect(customErrFn, "blah").to.not.throw();
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( customErrFn)*\] to not throw an error but 'CustomError: foo' was thrown$/
    );

    expect(() => {
      expect(badFn).to.not.throw(Error, "testing", "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw 'Error' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(badFn, "blah").to.not.throw(Error, "testing");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( badFn)*\] to not throw 'Error' but 'Error: testing' was thrown$/
    );

    expect(() => {
      expect(emptyStringErrFn).to.not.throw(Error, "", "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( emptyStringErrFn)*\] to not throw 'Error' but 'Error' was thrown$/
    );

    expect(() => {
      expect(emptyStringErrFn, "blah").to.not.throw(Error, "");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( emptyStringErrFn)*\] to not throw 'Error' but 'Error' was thrown$/
    );

    expect(() => {
      expect(emptyStringErrFn, "blah").to.not.throw("");
    }).to.throw(
      AssertionError,
      /^blah: expected \[Function( emptyStringErrFn)*\] to throw error not including ''$/
    );

    expect(() => {
      expect({}, "blah").to.throw();
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect({}).to.throw(Error, "testing", "blah");
    }).to.throw(AssertionError, "blah: expected {} to be a function");
  });

  it("respondTo", () => {
    function Foo() {}
    Foo.prototype.bar = function () {};
    Foo.func = function () {};

    var bar = {};
    bar.foo = function () {};

    expect(Foo).to.respondTo("bar");
    expect(Foo).to.not.respondTo("foo");
    expect(Foo).itself.to.respondTo("func");
    expect(Foo).itself.not.to.respondTo("bar");

    expect(bar).to.respondTo("foo");

    expect(() => {
      expect(Foo).to.respondTo("baz", "constructor");
    }).to.throw(
      AssertionError,
      /^(constructor: expected)(.*)(\[Function Foo\])(.*)(to respond to \'baz\')$/
    );

    expect(() => {
      expect(Foo, "constructor").to.respondTo("baz");
    }).to.throw(
      AssertionError,
      /^(constructor: expected)(.*)(\[Function Foo\])(.*)(to respond to \'baz\')$/
    );

    expect(() => {
      expect(bar).to.respondTo("baz", "object");
    }).to.throw(
      AssertionError,
      /^(object: expected)(.*)(\{ foo: \[Function\] \}|\{ Object \()(.*)(to respond to \'baz\')$/
    );

    expect(() => {
      expect(bar, "object").to.respondTo("baz");
    }).to.throw(
      AssertionError,
      /^(object: expected)(.*)(\{ foo: \[Function\] \}|\{ Object \()(.*)(to respond to \'baz\')$/
    );
  });

  it("satisfy", () => {
    var matcher = function (num) {
      return num === 1;
    };

    expect(1).to.satisfy(matcher);

    expect(() => {
      expect(2).to.satisfy(matcher, "blah");
    }).to.throw(
      AssertionError,
      /^blah: expected 2 to satisfy \[Function( matcher)*\]$/
    );

    expect(() => {
      expect(2, "blah").to.satisfy(matcher);
    }).to.throw(
      AssertionError,
      /^blah: expected 2 to satisfy \[Function( matcher)*\]$/
    );
  });

  it("closeTo", () => {
    expect(1.5).to.be.closeTo(1.0, 0.5);
    expect(10).to.be.closeTo(20, 20);
    expect(-10).to.be.closeTo(20, 30);
  });

  it("approximately", () => {
    expect(1.5).to.be.approximately(1.0, 0.5);
    expect(10).to.be.approximately(20, 20);
    expect(-10).to.be.approximately(20, 30);
  });

  it("oneOf", () => {
    expect(1).to.be.oneOf([1, 2, 3]);
    expect("1").to.not.be.oneOf([1, 2, 3]);
    expect([3, [4]]).to.not.be.oneOf([1, 2, [3, 4]]);
    var threeFour = [3, [4]];
    expect(threeFour).to.be.oneOf([1, 2, threeFour]);
  });

  it("include.members", () => {
    expect([1, 2, 3]).to.include.members([]);
    expect([1, 2, 3]).to.include.members([3, 2]);
    expect([1, 2, 3]).to.include.members([3, 2, 2]);
    expect([1, 2, 3]).to.not.include.members([8, 4]);
    expect([1, 2, 3]).to.not.include.members([1, 2, 3, 4]);
    expect([{ a: 1 }]).to.not.include.members([{ a: 1 }]);

    expect(() => {
      expect([1, 2, 3]).to.include.members([2, 5], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be a superset of [ 2, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").to.include.members([2, 5]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be a superset of [ 2, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3]).to.not.include.members([2, 1]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not be a superset of [ 2, 1 ]"
    );
  });

  it("same.members", () => {
    expect([5, 4]).to.have.same.members([4, 5]);
    expect([5, 4]).to.have.same.members([5, 4]);
    expect([5, 4, 4]).to.have.same.members([5, 4, 4]);
    expect([5, 4]).to.not.have.same.members([]);
    expect([5, 4]).to.not.have.same.members([6, 3]);
    expect([5, 4]).to.not.have.same.members([5, 4, 2]);
    expect([5, 4]).to.not.have.same.members([5, 4, 4]);
    expect([5, 4, 4]).to.not.have.same.members([5, 4]);
    expect([5, 4, 4]).to.not.have.same.members([5, 4, 3]);
    expect([5, 4, 3]).to.not.have.same.members([5, 4, 4]);
  });

  it("members", () => {
    expect([5, 4]).members([4, 5]);
    expect([5, 4]).members([5, 4]);
    expect([5, 4, 4]).members([5, 4, 4]);
    expect([5, 4]).not.members([]);
    expect([5, 4]).not.members([6, 3]);
    expect([5, 4]).not.members([5, 4, 2]);
    expect([5, 4]).not.members([5, 4, 4]);
    expect([5, 4, 4]).not.members([5, 4]);
    expect([5, 4, 4]).not.members([5, 4, 3]);
    expect([5, 4, 3]).not.members([5, 4, 4]);
    expect([{ id: 1 }]).not.members([{ id: 1 }]);

    expect(() => {
      expect([1, 2, 3]).members([2, 1, 5], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same members as [ 2, 1, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").members([2, 1, 5]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same members as [ 2, 1, 5 ]"
    );

    expect(() => {
      expect([1, 2, 3]).not.members([2, 1, 3]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not have the same members as [ 2, 1, 3 ]"
    );

    expect(() => {
      expect({}).members([], "blah");
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");

    expect(() => {
      expect({}, "blah").members([]);
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");

    expect(() => {
      expect([]).members({}, "blah");
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");

    expect(() => {
      expect([], "blah").members({});
    }).to.throw(AssertionError, "blah: expected {} to be an iterable");
  });

  it("deep.members", () => {
    expect([{ id: 1 }]).deep.members([{ id: 1 }]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).deep.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect([{ id: 2 }]).not.deep.members([{ id: 1 }]);
    expect([{ a: 1 }, { b: 2 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect(() => {
      expect([{ id: 1 }]).deep.members([{ id: 2 }], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ { id: 1 } ] to have the same members as [ { id: 2 } ]"
    );

    expect(() => {
      expect([{ id: 1 }], "blah").deep.members([{ id: 2 }]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { id: 1 } ] to have the same members as [ { id: 2 } ]"
    );
  });

  it("include.deep.members", () => {
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.members([
      { b: 2 },
      { a: 1 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.members([
      { b: 2 },
      { a: 1 },
      { a: 1 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.members([
      { b: 2 },
      { a: 1 },
      { f: 5 },
    ]);

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.members(
        [{ b: 2 }, { a: 1 }, { f: 5 }],
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be a superset of [ { b: 2 }, { a: 1 }, { f: 5 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }], "blah").include.deep.members([
        { b: 2 },
        { a: 1 },
        { f: 5 },
      ]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be a superset of [ { b: 2 }, { a: 1 }, { f: 5 } ]"
    );
  });

  it("ordered.members", () => {
    expect([1, 2, 3]).ordered.members([1, 2, 3]);
    expect([1, 2, 2]).ordered.members([1, 2, 2]);

    expect([1, 2, 3]).not.ordered.members([2, 1, 3]);
    expect([1, 2, 3]).not.ordered.members([1, 2]);
    expect([1, 2]).not.ordered.members([1, 2, 2]);
    expect([1, 2, 2]).not.ordered.members([1, 2]);
    expect([1, 2, 2]).not.ordered.members([1, 2, 3]);
    expect([1, 2, 3]).not.ordered.members([1, 2, 2]);

    expect(() => {
      expect([1, 2, 3]).ordered.members([2, 1, 3], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same ordered members as [ 2, 1, 3 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").ordered.members([2, 1, 3]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to have the same ordered members as [ 2, 1, 3 ]"
    );

    expect(() => {
      expect([1, 2, 3]).not.ordered.members([1, 2, 3]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not have the same ordered members as [ 1, 2, 3 ]"
    );
  });

  it("include.ordered.members", () => {
    expect([1, 2, 3]).include.ordered.members([1, 2]);
    expect([1, 2, 3]).not.include.ordered.members([2, 1]);
    expect([1, 2, 3]).not.include.ordered.members([2, 3]);
    expect([1, 2, 3]).not.include.ordered.members([1, 2, 2]);

    expect(() => {
      expect([1, 2, 3]).include.ordered.members([2, 1], "blah");
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be an ordered superset of [ 2, 1 ]"
    );

    expect(() => {
      expect([1, 2, 3], "blah").include.ordered.members([2, 1]);
    }).to.throw(
      AssertionError,
      "blah: expected [ 1, 2, 3 ] to be an ordered superset of [ 2, 1 ]"
    );

    expect(() => {
      expect([1, 2, 3]).not.include.ordered.members([1, 2]);
    }).to.throw(
      AssertionError,
      "expected [ 1, 2, 3 ] to not be an ordered superset of [ 1, 2 ]"
    );
  });

  it("deep.ordered.members", () => {
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.ordered.members([
      { b: 2 },
      { a: 1 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { b: 2 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).deep.ordered.members(
        [{ b: 2 }, { a: 1 }, { c: 3 }],
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to have the same ordered members as [ { b: 2 }, { a: 1 }, { c: 3 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }], "blah").deep.ordered.members([
        { b: 2 },
        { a: 1 },
        { c: 3 },
      ]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to have the same ordered members as [ { b: 2 }, { a: 1 }, { c: 3 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.deep.ordered.members([
        { a: 1 },
        { b: 2 },
        { c: 3 },
      ]);
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to not have the same ordered members as [ { a: 1 }, { b: 2 }, { c: 3 } ]"
    );
  });

  it("include.deep.ordered.members", () => {
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.ordered.members([
      { a: 1 },
      { b: 2 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
      { b: 2 },
      { a: 1 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
      { b: 2 },
      { c: 3 },
    ]);
    expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
      { a: 1 },
      { b: 2 },
      { b: 2 },
    ]);

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).include.deep.ordered.members(
        [{ b: 2 }, { a: 1 }],
        "blah"
      );
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be an ordered superset of [ { b: 2 }, { a: 1 } ]"
    );

    expect(() => {
      expect(
        [{ a: 1 }, { b: 2 }, { c: 3 }],
        "blah"
      ).include.deep.ordered.members([{ b: 2 }, { a: 1 }]);
    }).to.throw(
      AssertionError,
      "blah: expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to be an ordered superset of [ { b: 2 }, { a: 1 } ]"
    );

    expect(() => {
      expect([{ a: 1 }, { b: 2 }, { c: 3 }]).not.include.deep.ordered.members([
        { a: 1 },
        { b: 2 },
      ]);
    }).to.throw(
      AssertionError,
      "expected [ { a: 1 }, { b: 2 }, { c: 3 } ] to not be an ordered superset of [ { a: 1 }, { b: 2 } ]"
    );
  });

  it("change", () => {
    const obj = { value: 10, str: "foo" };
    const heroes = ["spiderman", "superman"];
    const fn = () => (obj.value += 5);
    const decFn = () => (obj.value -= 20);
    const sameFn = () => "foo" + "bar";
    const bangFn = () => (obj.str += "!");
    const batFn = () => heroes.push("batman");
    const lenFn = () => heroes.length;

    expect(fn).to.change(obj, "value");
    expect(fn).to.change(obj, "value").by(5);
    expect(fn).to.change(obj, "value").by(-5);

    expect(decFn).to.change(obj, "value").by(20);
    expect(decFn).to.change(obj, "value").but.not.by(21);

    expect(sameFn).to.not.change(obj, "value");

    expect(sameFn).to.not.change(obj, "str");
    expect(bangFn).to.change(obj, "str");

    expect(batFn).to.change(lenFn).by(1);
    expect(batFn).to.change(lenFn).but.not.by(2);

    expect(() => {
      expect(sameFn).to.change(obj, "value", "blah");
    }).to.throw(AssertionError, "blah: expected .value to change");

    expect(() => {
      expect(sameFn, "blah").to.change(obj, "value");
    }).to.throw(AssertionError, "blah: expected .value to change");

    expect(() => {
      expect(fn).to.not.change(obj, "value", "blah");
    }).to.throw(AssertionError, "blah: expected .value to not change");

    expect(() => {
      expect({}).to.change(obj, "value", "blah");
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect({}, "blah").to.change(obj, "value");
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect(fn).to.change({}, "badprop", "blah");
    }).to.throw(AssertionError, "blah: expected {} to have property 'badprop'");

    expect(() => {
      expect(fn, "blah").to.change({}, "badprop");
    }).to.throw(AssertionError, "blah: expected {} to have property 'badprop'");

    expect(() => {
      expect(fn, "blah").to.change({});
    }).to.throw(AssertionError, "blah: expected {} to be a function");

    expect(() => {
      expect(fn).to.change(obj, "value").by(10, "blah");
    }).to.throw(AssertionError, "blah: expected .value to change by 10");

    expect(() => {
      expect(fn, "blah").to.change(obj, "value").by(10);
    }).to.throw(AssertionError, "blah: expected .value to change by 10");

    expect(() => {
      expect(fn).to.change(obj, "value").but.not.by(5, "blah");
    }).to.throw(AssertionError, "blah: expected .value to not change by 5");
  });

  it("increase, decrease", () => {
    var obj = { value: 10, noop: null },
      arr = ["one", "two"],
      pFn = function () {
        arr.push("three");
      },
      popFn = function () {
        arr.pop();
      },
      nFn = function () {
        return null;
      },
      lenFn = function () {
        return arr.length;
      },
      incFn = function () {
        obj.value += 2;
      },
      decFn = function () {
        obj.value -= 3;
      },
      smFn = function () {
        obj.value += 0;
      };

    expect(smFn).to.not.increase(obj, "value");
    expect(decFn).to.not.increase(obj, "value");
    expect(incFn).to.increase(obj, "value");
    expect(incFn).to.increase(obj, "value").by(2);
    expect(incFn).to.increase(obj, "value").but.not.by(1);

    expect(smFn).to.not.decrease(obj, "value");
    expect(incFn).to.not.decrease(obj, "value");
    expect(decFn).to.decrease(obj, "value");
    expect(decFn).to.decrease(obj, "value").by(3);
    expect(decFn).to.decrease(obj, "value").but.not.by(2);

    expect(popFn).to.not.increase(lenFn);
    expect(nFn).to.not.increase(lenFn);
    expect(pFn).to.increase(lenFn);
    expect(pFn).to.increase(lenFn).by(1);
    expect(pFn).to.increase(lenFn).but.not.by(2);

    expect(popFn).to.decrease(lenFn);
    expect(popFn).to.decrease(lenFn).by(1);
    expect(popFn).to.decrease(lenFn).but.not.by(2);
    expect(nFn).to.not.decrease(lenFn);
    expect(pFn).to.not.decrease(lenFn);
  });

  it("extensible", function () {
    const nonExtensibleObject = Object.preventExtensions({});

    expect({}).to.be.extensible;
    expect(nonExtensibleObject).to.not.be.extensible;

    expect(() => {
      expect(nonExtensibleObject, "blah").to.be.extensible;
    }).to.throw(AssertionError, "blah: expected {} to be extensible");

    expect(() => {
      expect({}).to.not.be.extensible;
    }).to.throw(AssertionError, "expected {} to not be extensible");

    expect(42).to.not.be.extensible;
    expect(null).to.not.be.extensible;
    expect("foo").to.not.be.extensible;
    expect(false).to.not.be.extensible;
    expect(undefined).to.not.be.extensible;
    expect(sym).to.not.be.extensible;

    expect(() => {
      expect(42).to.be.extensible;
    }).to.throw(AssertionError, "expected 42 to be extensible");

    expect(() => {
      expect(null).to.be.extensible;
    }).to.throw(AssertionError, "expected null to be extensible");

    expect(() => {
      expect("foo").to.be.extensible;
    }).to.throw(AssertionError, "expected 'foo' to be extensible");

    expect(() => {
      expect(false).to.be.extensible;
    }).to.throw(AssertionError, "expected false to be extensible");

    expect(() => {
      expect(undefined).to.be.extensible;
    }).to.throw(AssertionError, "expected undefined to be extensible");

    const proxy = new Proxy(
      {},
      {
        isExtensible() {
          throw new TypeError();
        },
      }
    );

    expect(() => {
      expect(proxy).to.be.extensible;
    }).to.throw(TypeError);
  });

  it("sealed", function () {
    const sealedObject = Object.seal({});

    expect(sealedObject).to.be.sealed;
    expect({}).to.not.be.sealed;

    expect(() => {
      expect({}).to.be.sealed;
    }).to.throw(AssertionError, "expected {} to be sealed");

    expect(() => {
      expect(sealedObject).to.not.be.sealed;
    }).to.throw(AssertionError, "expected {} to not be sealed");

    expect(42).to.be.sealed;
    expect(null).to.be.sealed;
    expect("foo").to.be.sealed;
    expect(false).to.be.sealed;
    expect(undefined).to.be.sealed;
    expect(sym).to.be.sealed;

    expect(() => {
      expect(42).to.not.be.sealed;
    }).to.throw(AssertionError, "expected 42 to not be sealed");

    expect(() => {
      expect(null).to.not.be.sealed;
    }).to.throw(AssertionError, "expected null to not be sealed");

    expect(() => {
      expect("foo").to.not.be.sealed;
    }).to.throw(AssertionError, "expected 'foo' to not be sealed");

    expect(() => {
      expect(false).to.not.be.sealed;
    }).to.throw(AssertionError, "expected false to not be sealed");

    expect(() => {
      expect(undefined).to.not.be.sealed;
    }).to.throw(AssertionError, "expected undefined to not be sealed");

    const proxy = new Proxy(
      {},
      {
        ownKeys() {
          throw new TypeError();
        },
      }
    );

    Object.preventExtensions(proxy);

    expect(() => {
      expect(proxy).to.be.sealed;
    }).to.throw(TypeError);
  });

  it("frozen", function () {
    const frozenObject = Object.freeze({});

    expect(frozenObject).to.be.frozen;
    expect({}).to.not.be.frozen;

    expect(() => {
      expect({}).to.be.frozen;
    }).to.throw(AssertionError, "expected {} to be frozen");

    expect(() => {
      expect(frozenObject).to.not.be.frozen;
    }).to.throw(AssertionError, "expected {} to not be frozen");

    expect(42).to.be.frozen;
    expect(null).to.be.frozen;
    expect("foo").to.be.frozen;
    expect(false).to.be.frozen;
    expect(undefined).to.be.frozen;
    expect(sym).to.be.frozen;

    expect(() => {
      expect(42).to.not.be.frozen;
    }).to.throw(AssertionError, "expected 42 to not be frozen");

    expect(() => {
      expect(null).to.not.be.frozen;
    }).to.throw(AssertionError, "expected null to not be frozen");

    expect(() => {
      expect("foo").to.not.be.frozen;
    }).to.throw(AssertionError, "expected 'foo' to not be frozen");

    expect(() => {
      expect(false).to.not.be.frozen;
    }).to.throw(AssertionError, "expected false to not be frozen");

    expect(() => {
      expect(undefined).to.not.be.frozen;
    }).to.throw(AssertionError, "expected undefined to not be frozen");

    const proxy = new Proxy(
      {},
      {
        ownKeys() {
          throw new TypeError();
        },
      }
    );

    Object.preventExtensions(proxy);

    expect(() => {
      expect(proxy).to.be.frozen;
    }).to.throw(TypeError);
  });
});

function runTest(fileData) {
  tests.forEach((test) => test.func());
}

})();

self.WTBenchmark = __webpack_exports__;
/******/ })()
;