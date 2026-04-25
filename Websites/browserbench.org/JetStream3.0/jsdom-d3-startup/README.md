# JSDOM D3 Startup Benchmark

The benchmark reads airport and US geography data, then uses D3 to create a Voronoi diagram of the airports overlaid on a map of the US.
It uses jsdom to simulate a browser environment for D3 to render to an SVG element.

## JetStream integration
- We use a custom `./build/build/cache-buster-comment-plugin.cjs` which injects a known comment into every function in the bundle
- The JetStream benchmark replaces these comments with a unique string per iteration
- Each benchmark iteration includes parse and top-level eval time

## Setup
```bash
# Install node deps from package-lock.json
npm ci --include=dev; 
# Bundle sources to dist/*.
npm run build
# Use build:dev for non-minified sources.
npm run build:dev
```

# Testing
```bash
# Run the basic node benchmark implementation for development. 
npm run test
```
