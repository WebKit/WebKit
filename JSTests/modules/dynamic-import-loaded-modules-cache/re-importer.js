// This module performs its own dynamic import of leaf.js so that the
// LoadedModules cache check in hostLoadImportedModule is exercised with
// a CyclicModuleRecord referrer (this module), not just the Realm.
export const ns = await import("./leaf.js");
