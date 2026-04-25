# validator.js test for JetStream

Measures the performance of the validator.js library by running the core test suite.

## Build Instructions

```bash
# install required node packages.
npm ci
# build the workload, output is ./dist
npm run build
```

## Workloads

The workload is a modified copy of the validator.js [test suite](https://github.com/validatorjs/validator.js/tree/master/test).
Several files are combined together and contents copied to the `runTest` method.

Modifications to the original:
- Any tests that depend on node internal values have been removed.
- Date constructions with an arg-array differs between browser and thus they
  are replaced by direct arguments.
- Direct calls to validater.xzy() are replaced with validatorjs.xzy() to not
  conflict with the default argument to the mock test() method.
