//@ requireOptions("--useImportAttributes=1")
import x from './resources/x.js' with {};
import './resources/y.js' with {};
export * from './resources/z.js' with {};
