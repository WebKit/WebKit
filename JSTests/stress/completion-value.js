function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeWithValueCheck(actual, callback) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Basic.
shouldBe(eval(), undefined);
shouldBe(eval(""), undefined);
shouldBe(eval(`1`), 1);
shouldBe(eval(`1; 2`), 2);

// Types of statements:
// https://tc39.github.io/ecma262/#prod-Statement

// Statements that produce an (empty) completion value do not affect results:
//   - EmptyStatement
//   - DebuggerStatement
//   - BlockStatement (with no substatement)
//   - DeclarationStatements (variables, functions, classes)
//   - LabelledStatement (with an empty statement)
//   - ContinueStatement / BreakStatement (tested below)
shouldBe(eval(`42;`), 42);
shouldBe(eval(`42;;;;;`), 42);
shouldBe(eval(`42; var x;`), 42);
shouldBe(eval(`42; let x;`), 42);
shouldBe(eval(`42; const x = 1;`), 42);
shouldBe(eval(`42; var x = 1;`), 42);
shouldBe(eval(`42; var x = 1, y = 2;`), 42);
shouldBe(eval(`42; debugger;`), 42);
shouldBe(eval(`42; { }`), 42);
shouldBe(eval(`42; class foo {}`), 42);
shouldBe(eval(`42; function foo() {}`), 42);
shouldBe(eval(`42; function* foo() {}`), 42);
shouldBe(eval(`42; async function foo() {}`), 42);
shouldBe(eval(`42; label: {}`), 42);
shouldBe(eval(`42; { { var x; } var y; { { debugger } ;; } function foo() {} }`), 42);

// ExpressionStatement
shouldBe(eval(`99; 42`), 42);
shouldBe(eval(`99; { 42 }`), 42);
shouldBe(eval(`99; label: { 42 }`), 42);
shouldBe(eval(`99; x = 42`), 42);
shouldBe(eval(`99; x = 42; x`), 42);
shouldBe(eval(`99; 1, 2, 3, 42`), 42);
shouldBe(eval(`99; x = 41; ++x`), 42);
shouldBe(eval(`99; x = 42; x++`), 42);
shouldBe(eval(`99; true ? 42 : -1`), 42);
shouldBe(eval(`99; false ? -1 : 42`), 42);
shouldBe(Array.isArray(eval(`99; [x,y] = [1,2]`)), true);
shouldBe(typeof eval(`99; ({})`), "object");
shouldBe(typeof eval(`99; (function foo() {})`), "function");
shouldBe(typeof eval(`99; (function* foo() {})`), "function");
shouldBe(typeof eval(`99; (async function foo() {})`), "function");
shouldBe(typeof eval(`99; (class foo {})`), "function");

// IfStatement
shouldBe(eval(`99; if (true);`), undefined);
shouldBe(eval(`99; if (false);`), undefined);
shouldBe(eval(`99; if (true) 42;`), 42);
shouldBe(eval(`99; if (false) -1;`), undefined);
shouldBe(eval(`99; if (true) {}`), undefined);
shouldBe(eval(`99; if (true) {42}`), 42);
shouldBe(eval(`99; if (false) {}`), undefined);
shouldBe(eval(`99; if (false) {42}`), undefined);
shouldBe(eval(`99; if (false) {-1} else {}`), undefined);
shouldBe(eval(`99; if (false) {-1} else {42}`), 42);
shouldBe(eval(`99; if (false) {-1} else if (true) {}`), undefined);
shouldBe(eval(`99; if (false) {-1} else if (true) {42}`), 42);
shouldBe(eval(`99; if (false) {-1} else if (true) {} else {-2}`), undefined);
shouldBe(eval(`99; if (false) {-1} else if (true) {42} else {-2}`), 42);
shouldBe(eval(`99; if (false) {-1} else if (false) {-2} else {}`), undefined);
shouldBe(eval(`99; if (false) {-1} else if (false) {-2} else {42}`), 42);

// DoWhile
shouldBe(eval(`99; do; while (false);`), undefined);
shouldBe(eval(`99; do 42; while (false);`), 42);
shouldBe(eval(`99; do {} while (false);`), undefined);
shouldBe(eval(`99; do break; while (true);`), undefined);
shouldBe(eval(`99; do { break } while (true);`), undefined);
shouldBe(eval(`99; do { 42 } while (false);`), 42);
shouldBe(eval(`let x = 1; do { x } while (x++ !== (5+5));`), 10);
shouldBe(eval(`let x = 1; do { x; 42 } while (x++ !== (5+5));`), 42);
shouldBe(eval(`let x = 1; do { x; break } while (x++ !== (5+5));`), 1);
shouldBe(eval(`let x = 1; do { x; continue } while (x++ !== (5+5));`), 10);

// While
shouldBe(eval(`99; while (false);`), undefined);
shouldBe(eval(`99; while (false) {};`), undefined);
shouldBe(eval(`99; while (true) break;`), undefined);
shouldBe(eval(`99; while (true) { break };`), undefined);
shouldBe(eval(`99; while (true) { 42; break };`), 42);
shouldBe(eval(`let x = 1; while (x++ !== (5+5)) ;`), undefined);
shouldBe(eval(`let x = 1; while (x++ !== (5+5)) { }`), undefined);
shouldBe(eval(`let x = 1; while (x++ !== (5+5)) { x }`), 10);
shouldBe(eval(`let x = 1; while (x++ !== (5+5)) { x; 42 }`), 42);
shouldBe(eval(`let x = 1; while (x++ !== (5+5)) { x; break }`), 2);
shouldBe(eval(`let x = 1; while (x++ !== (5+5)) { x; continue }`), 10);

// For
shouldBe(eval(`99; for (;false;);`), undefined);
shouldBe(eval(`99; for (;false;) {}`), undefined);
shouldBe(eval(`99; for (var x = 1;false;);`), undefined);
shouldBe(eval(`99; for (x = 1;false;) {}`), undefined);
shouldBe(eval(`99; for (;;) break;`), undefined);
shouldBe(eval(`99; for (;;) { break }`), undefined);
shouldBe(eval(`99; for (;;) { 42; break }`), 42);
shouldBe(eval(`99; for (;;) { 42; break; 3 }`), 42);
shouldBe(eval(`99; for (x = 1; x !== (5+5); x++) x;`), 9);
shouldBe(eval(`99; for (x = 1; x !== (5+5); x++) { x; }`), 9);
shouldBe(eval(`99; for (x = 1; x !== (5+5); x++) { x; 42 }`), 42);
shouldBe(eval(`99; for (x = 1; x !== (5+5); x++) { x; break }`), 1);
shouldBe(eval(`99; for (x = 1; x !== (5+5); x++) { x; break; 3 }`), 1);
shouldBe(eval(`99; for (x = 1; x !== (5+5); x++) { x; continue }`), 9);
shouldBe(eval(`99; for (x = 1; x !== (5+5); x++) { x; continue; 3 }`), 9);

// ForOf
shouldBe(eval(`99; for (var x of []) -1;`), undefined);
shouldBe(eval(`99; for (x of []) -1;`), undefined);
shouldBe(eval(`99; for (var x of [1,2]);`), undefined);
shouldBe(eval(`99; for (x of [1,2]);`), undefined);
shouldBe(eval(`99; for (x of [1,2]) {}`), undefined);
shouldBe(eval(`99; for (x of [1,2]) break;`), undefined);
shouldBe(eval(`99; for (x of [1,2]) { break; }`), undefined);
shouldBe(eval(`99; for (x of [1,2]) { break; 3; }`), undefined);
shouldBe(eval(`99; for (x of [1,2]) x`), 2);
shouldBe(eval(`99; for (x of [1,2]) { x }`), 2);
shouldBe(eval(`99; for (x of [1,2]) { x; break }`), 1);
shouldBe(eval(`99; for (x of [1,2]) { x; break; 3 }`), 1);
shouldBe(eval(`99; for (x of [1,2]) { x; continue }`), 2);
shouldBe(eval(`99; for (x of [1,2]) { x; continue; 3 }`), 2);

// ForIn
shouldBe(eval(`99; for (var x in {}) -1;`), undefined);
shouldBe(eval(`99; for (x in {}) -1;`), undefined);
shouldBe(eval(`99; for (var x in {a:1,b:2});`), undefined);
shouldBe(eval(`99; for (x in {a:1,b:2});`), undefined);
shouldBe(eval(`99; for (x in {a:1,b:2}) {}`), undefined);
shouldBe(eval(`99; for (x in {a:1,b:2}) break;`), undefined);
shouldBe(eval(`99; for (x in {a:1,b:2}) { break; }`), undefined);
shouldBe(eval(`99; for (x in {a:1,b:2}) { break; 3; }`), undefined);
shouldBe(eval(`99; for (x in {a:1,b:2}) x`), "b");
shouldBe(eval(`99; for (x in {a:1,b:2}) { x }`), "b");
shouldBe(eval(`99; for (x in {a:1,b:2}) { x; break }`), "a");
shouldBe(eval(`99; for (x in {a:1,b:2}) { x; break; 3 }`), "a");
shouldBe(eval(`99; for (x in {a:1,b:2}) { x; continue }`), "b");
shouldBe(eval(`99; for (x in {a:1,b:2}) { x; continue; 3 }`), "b");

// SwitchStatement
shouldBe(eval(`99; switch (1) { }`), undefined);
shouldBe(eval(`99; switch (1) { default:}`), undefined);
shouldBe(eval(`99; switch (1) { default:42}`), 42);
shouldBe(eval(`99; switch (1) { default:break;}`), undefined);
shouldBe(eval(`99; switch (1) { case 1: /* empty */ }`), undefined);
shouldBe(eval(`99; switch (1) { case 1: 42 }`), 42);
shouldBe(eval(`99; switch (1) { case 1: break; }`), undefined);
shouldBe(eval(`99; switch (1) { case 1: break; }`), undefined);
shouldBe(eval(`99; switch (1) { case 2: case 1: /* empty */ }`), undefined);
shouldBe(eval(`99; switch (1) { case 2: case 1: 42 }`), 42);
shouldBe(eval(`99; switch (1) { case 2: case 1: break; }`), undefined);
shouldBe(eval(`99; switch (1) { case 2: case 1: 42; break; }`), 42);
shouldBe(eval(`99; switch (1) { case 1: case 2: /* empty */ }`), undefined);
shouldBe(eval(`99; switch (1) { case 1: case 2: 42 }`), 42);
shouldBe(eval(`99; switch (1) { case 1: case 2: break; }`), undefined);
shouldBe(eval(`99; switch (1) { case 1: case 2: 42; break; }`), 42);
shouldBe(eval(`99; switch (1) { case 1: 42; case 2: /* empty */ }`), 42);
shouldBe(eval(`99; switch (1) { case 1: -1; case 2: 42 }`), 42);
shouldBe(eval(`99; switch (1) { case 1: 42; case 2: break; }`), 42);
shouldBe(eval(`99; switch (1) { case 1: -1; case 2: 42; break; }`), 42);
shouldBe(eval(`99; switch (1) { case 0: -1; break; case 1: 42; break; default: -1; break; }`), 42);
shouldBe(eval(`99; switch (1) { case 0: -1; break; case 1: /* empty */; break; default: -1; break; }`), undefined);
shouldBe(eval(`99; switch (1) { case 0: -1; break; case 1: 42; break; default: -1; break; }`), 42);

// WithStatement
shouldBe(eval(`99; with (true);`), undefined);
shouldBe(eval(`99; with (true) {}`), undefined);
shouldBe(eval(`99; with (true) 42;`), 42);
shouldBe(eval(`99; with (true) { 42 }`), 42);

// TryCatchFinally / ThrowStatement
shouldBe(eval(`99; try {} catch (e) {-1};`), undefined);
shouldBe(eval(`99; try {} catch (e) {-1} finally {-2};`), undefined);
shouldBe(eval(`99; try {42} catch (e) {-1};`), 42);
shouldBe(eval(`99; try {42} catch (e) {-1} finally {-2};`), 42);
shouldBe(eval(`99; try { [].x.x } catch (e) {};`), undefined);
shouldBe(eval(`99; try { [].x.x } catch (e) {42} finally {-2};`), 42);
shouldBe(eval(`99; try { throw 42 } catch (e) {e};`), 42);
shouldBe(eval(`99; try { throw 42 } catch (e) {e} finally {-2};`), 42);

// Break Statement where it is not normally available.
shouldBe(eval(`99; do { -99; if (true) { break; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (true) { 42; break; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; if (false) { } else { break; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (false) { } else { 42; break; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; with (true) { break; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; with (true) { 42; break; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { break; } catch (e) { -1 }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; break; } catch (e) { -1 }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { break; } catch (e) {-1} finally {-2}; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; break; } catch (e) {-1} finally {-2}; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { break; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; break; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { break; } finally {-2}; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; break; } finally {-2}; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { 42 } catch (e) { -1 } finally { -2; break; -3 }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; } finally { -2; break; -3 }; } while (false);`), 42);

// Break Statement where it is not normally available with other surrounding statements.
shouldBe(eval(`99; do { -99; if (true) { break; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (true) { 42; break; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; if (false) { } else { break; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (false) { } else { 42; break; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; with (true) { break; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; with (true) { 42; break; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { break; } catch (e) { -1 }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; break; } catch (e) { -1 }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { break; } catch (e) {-1} finally {-2}; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; break; } catch (e) {-1} finally {-2}; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { break; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; break; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { break; } finally {-2}; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; break; } finally {-2}; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { 42 } catch (e) { -1 } finally { -2; break; -3 }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; } finally { -2; break; -3 }; -77 } while (false);`), 42);

// Continue Statement where it is not normally available.
shouldBe(eval(`99; do { -99; if (true) { continue; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (true) { 42; continue; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; if (false) { } else { continue; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (false) { } else { 42; continue; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; with (true) { continue; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; with (true) { 42; continue; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { continue; } catch (e) { -1 }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; continue; } catch (e) { -1 }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { continue; } catch (e) {-1} finally {-2}; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; continue; } catch (e) {-1} finally {-2}; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { continue; }; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; continue; }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { continue; } finally {-2}; } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; continue; } finally {-2}; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { 42 } catch (e) { -1 } finally { -2; continue; -3 }; } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; } finally { -2; continue; -3 }; } while (false);`), 42);

// Continue Statement where it is not normally available with other surrounding statements.
shouldBe(eval(`99; do { -99; if (true) { continue; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (true) { 42; continue; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; if (false) { } else { continue; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; if (false) { } else { 42; continue; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; with (true) { continue; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; with (true) { 42; continue; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { continue; } catch (e) { -1 }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; continue; } catch (e) { -1 }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { continue; } catch (e) {-1} finally {-2}; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { 42; continue; } catch (e) {-1} finally {-2}; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { continue; }; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; continue; }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { continue; } finally {-2}; -77 } while (false);`), undefined);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; continue; } finally {-2}; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { 42 } catch (e) { -1 } finally { -2; continue; -3 }; -77 } while (false);`), 42);
shouldBe(eval(`99; do { -99; try { [].x.x } catch (e) { 42; } finally { -2; continue; -3 }; -77 } while (false);`), 42);

// Early break to a label.
shouldBe(eval(`99; label: do { 1; if (true) { break label; 2; }; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; if (true) { break label; 2; }; 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; with (true) { break label; 2; }; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; with (true) { break label; 2; }; 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; do { break label; 2; } while (false); } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; do { break label; 2; } while (false); 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; try { break label; 2; } catch (e) {-1;} while (false); } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; try { break label; 2; } catch (e) {-1;} while (false); 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; while (true) { break label; 2; }; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; while (true) { break label; 2; }; 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; for (;;) { break label; 2; }; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; for (;;) { break label; 2; }; 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; for (var x in {a:77}) { break label; 2; }; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; for (var x in {a:77}) { break label; 2; }; 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; for (var x of [77]) { break label; 2; }; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; for (var x of [77]) { break label; 2; }; 3; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; switch (true) { default: break label; 2; }; } while (false);`), undefined);
shouldBe(eval(`99; label: do { 1; switch (true) { default: break label; 2; }; 3; } while (false);`), undefined);

// FIXME: Module Only Statements:
// FIXME: ImportStatement
// FIXME: ExportStatement
