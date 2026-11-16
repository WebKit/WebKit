//@ runDefault("--maxPerThreadStackUsage=160000")

// The module graph below is a wide fan-out onto a long linear chain:
//
//   root.js: import './a0.js'; import './a1.js'; ... import './a150.js';
//   a{i}.js: import './a{i+1}.js';
//   a150.js: export const leaf = true;
//
// Because root requests every a{i}, each a{i}'s map entry gets a record set
// as soon as its subgraph is loaded, which happens leaf-first. When a{i}'s
// InnerModuleLoading then requests a{i+1}, hostLoadImportedModule finds the
// cached record and synchronously re-enters through FinishLoadingImportedModule
// -> ContinueModuleLoading -> InnerModuleLoading, producing an unbounded
// mutually-recursive cycle. Without a stack guard at InnerModuleLoading entry,
// this crashes with a hard stack overflow once the graph is deep enough; with
// the guard it throws RangeError.
//
// This test only asserts that we do not crash. Both a successful load and a
// caught RangeError are acceptable outcomes.

import("./resources/module-graph-loading-stack-overflow/root.js").then(
    () => { },
    (error) => {
        if (!(error instanceof RangeError))
            throw error;
    });
