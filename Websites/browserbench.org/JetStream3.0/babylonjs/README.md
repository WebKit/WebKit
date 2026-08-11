# Babylon.js Benchmarks for JetStream

This project contains two benchmarks for testing the performance of the Babylon.js 3D engine.

## Build Instructions

```bash
# install required node packages.
npm ci
# build the workload, output is ./dist
npm run build
```

## Workloads

There are two distinct workloads in this benchmark suite:

### 1. Startup Workload

This benchmark measures the time it takes for the Babylon.js engine to initialize. It evaluates a large, bundled source file and measures the time to parse the code and execute a simple test. This workload is primarily focused on parse and startup time.

To run this benchmark in node for testing:
```bash
npm run test:startup
```

### 2. Scene Workload

This benchmark measures the rendering performance of a complex 3D scene. It loads 3D models (`.glb` files), animations, and particle systems, and then renders the scene for a number of frames. 

To run this benchmark in node for testing:
```bash
npm run test:scene
```
