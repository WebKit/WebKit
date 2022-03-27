/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { mergeParams } from '../internal/params_utils.js'; // ================================================================
// "Public" ParamsBuilder API / Documentation
// ================================================================

/**
 * Provides doc comments for the methods of CaseParamsBuilder and SubcaseParamsBuilder.
 * (Also enforces rough interface match between them.)
 */

/**
 * Base class for `CaseParamsBuilder` and `SubcaseParamsBuilder`.
 */
export class ParamsBuilderBase {
  constructor(cases) {
    this.cases = cases;
  }

  /**
   * Hidden from test files. Use `builderIterateCasesWithSubcases` to access this.
   */
}

/**
 * Calls the (normally hidden) `iterateCasesWithSubcases()` method.
 */
export function builderIterateCasesWithSubcases(builder) {
  return builder.iterateCasesWithSubcases();
}

/**
 * Builder for combinatorial test **case** parameters.
 *
 * CaseParamsBuilder is immutable. Each method call returns a new, immutable object,
 * modifying the list of cases according to the method called.
 *
 * This means, for example, that the `unit` passed into `TestBuilder.params()` can be reused.
 */
export class CaseParamsBuilder extends ParamsBuilderBase {
  *iterateCasesWithSubcases() {
    for (const a of this.cases()) {
      yield [a, undefined];
    }
  }

  [Symbol.iterator]() {
    return this.cases();
  }

  /** @inheritdoc */
  expandWithParams(expander) {
    const newGenerator = expanderGenerator(this.cases, expander);
    return new CaseParamsBuilder(() => newGenerator({}));
  }

  /** @inheritdoc */
  expand(key, expander) {
    return this.expandWithParams(function* (p) {
      for (const value of expander(p)) {
        // TypeScript doesn't know here that NewPKey is always a single literal string type.
        yield { [key]: value };
      }
    });
  }

  /** @inheritdoc */
  combineWithParams(newParams) {
    return this.expandWithParams(() => newParams);
  }

  /** @inheritdoc */
  combine(key, values) {
    return this.expand(key, () => values);
  }

  /** @inheritdoc */
  filter(pred) {
    const newGenerator = filterGenerator(this.cases, pred);
    return new CaseParamsBuilder(() => newGenerator({}));
  }

  /** @inheritdoc */
  unless(pred) {
    return this.filter(x => !pred(x));
  }

  /**
   * "Finalize" the list of cases and begin defining subcases.
   * Returns a new SubcaseParamsBuilder. Methods called on SubcaseParamsBuilder
   * generate new subcases instead of new cases.
   */
  beginSubcases() {
    return new SubcaseParamsBuilder(
      () => this.cases(),
      function* () {
        yield {};
      }
    );
  }
}

/**
 * The unit CaseParamsBuilder, representing a single case with no params: `[ {} ]`.
 *
 * `punit` is passed to every `.params()`/`.paramsSubcasesOnly()` call, so `kUnitCaseParamsBuilder`
 * is only explicitly needed if constructing a ParamsBuilder outside of a test builder.
 */
export const kUnitCaseParamsBuilder = new CaseParamsBuilder(function* () {
  yield {};
});

/**
 * Builder for combinatorial test _subcase_ parameters.
 *
 * SubcaseParamsBuilder is immutable. Each method call returns a new, immutable object,
 * modifying the list of subcases according to the method called.
 */
export class SubcaseParamsBuilder extends ParamsBuilderBase {
  constructor(cases, generator) {
    super(cases);
    this.subcases = generator;
  }

  *iterateCasesWithSubcases() {
    for (const caseP of this.cases()) {
      const subcases = Array.from(this.subcases(caseP));
      if (subcases.length) {
        yield [caseP, subcases];
      }
    }
  }

  /** @inheritdoc */
  expandWithParams(expander) {
    return new SubcaseParamsBuilder(this.cases, expanderGenerator(this.subcases, expander));
  }

  /** @inheritdoc */
  expand(key, expander) {
    return this.expandWithParams(function* (p) {
      for (const value of expander(p)) {
        // TypeScript doesn't know here that NewPKey is always a single literal string type.
        yield { [key]: value };
      }
    });
  }

  /** @inheritdoc */
  combineWithParams(newParams) {
    return this.expandWithParams(() => newParams);
  }

  /** @inheritdoc */
  combine(key, values) {
    return this.expand(key, () => values);
  }

  /** @inheritdoc */
  filter(pred) {
    return new SubcaseParamsBuilder(this.cases, filterGenerator(this.subcases, pred));
  }

  /** @inheritdoc */
  unless(pred) {
    return this.filter(x => !pred(x));
  }
}

function expanderGenerator(baseGenerator, expander) {
  return function* (base) {
    for (const a of baseGenerator(base)) {
      for (const b of expander(mergeParams(base, a))) {
        yield mergeParams(a, b);
      }
    }
  };
}

function filterGenerator(baseGenerator, pred) {
  return function* (base) {
    for (const a of baseGenerator(base)) {
      if (pred(mergeParams(base, a))) {
        yield a;
      }
    }
  };
}
