//@ requireOptions("--useSourcePhaseImports=1")

checkModuleSyntax(`import source x from "mod"`);
checkModuleSyntax(`import source from "mod"`);
checkModuleSyntax(`import {source} from "mod"`);
checkModuleSyntax(`import source, * as ns from "mod"`);
checkModuleSyntax(`import source x from "mod" with { type: "json" }`);
