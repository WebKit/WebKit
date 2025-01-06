//@ requireOptions("--useImportDefer=1")

checkModuleSyntax(`import defer * as ns from "mod"`);
checkModuleSyntax(`import defer from "mod"`);
checkModuleSyntax(`import defer, * as ns from "mod"`);
checkModuleSyntax(`import defer, { e1, e2 } from "mod"`);
checkModuleSyntax(`import {defer} from "mod"`);
checkModuleSyntax(`import defer * as defer from "mod"`);
