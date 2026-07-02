# TypeSCript test for JetStream

Measures the performance of running typescript on several framwork sources.

The build steps bundles sources from different frameworks:
- [jestjs](https://github.com/jestjs/jest.git) 
- [zod](https://github.com/colinhacks/zod.git)
- [immer](https://github.com/immerjs/immer.git)

## Build Instructions

```bash
# install required node packages.
npm ci
# build the workload, output is ./dist
npm run build
```