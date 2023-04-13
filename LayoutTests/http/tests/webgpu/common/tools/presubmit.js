/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { DefaultTestFileLoader } from '../internal/file_loader.js';import { parseQuery } from '../internal/query/parseQuery.js';import { assert } from '../util/util.js';

void (async () => {
  for (const suite of ['unittests', 'webgpu']) {
    const loader = new DefaultTestFileLoader();
    const filterQuery = parseQuery(`${suite}:*`);
    const testcases = await loader.loadCases(filterQuery);
    for (const testcase of testcases) {
      const name = testcase.query.toString();
      const maxLength = 375;
      assert(
      name.length <= maxLength,
      `Testcase ${name} is too long. Max length is ${maxLength} characters. Please shorten names or reduce parameters.`);

    }
  }
})();
//# sourceMappingURL=presubmit.js.map