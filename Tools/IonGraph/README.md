# IonGraph

JavaScriptCore visualizer for control flow graph.
Forked version of [iongraph](https://github.com/mozilla-spidermonkey/iongraph), an interactive visualizer for the SpiderMonkey Ion compiler backend.

Main differences are,

1. Because JSC's graph are irreducible in many cases, we are using traditional layout mechanism, and incorporating iongraph's better layout heuristics into it.
2. loop header, backedge etc. are computed in the visualizer side, so graph JSON does not need to include them. Sometimes, JSC graph is not ready to generate this information from C++ because predecessors are not set up yet.
3. Make it work without using ES6 modules for dist-www built version so that we can just use it by opening a local HTML file without a server.

## Developing

To run the viewer locally:

```
bun install
bun run serve
```
