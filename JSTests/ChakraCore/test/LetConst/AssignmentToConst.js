//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try { eval("WScript.Echo('test 1'); const x = 1; x = 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 2'); const x = 1; x += 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 3'); const x = 1; x -= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 4'); const x = 1; x *= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 5'); const x = 1; x /= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 6'); const x = 1; x &= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 7'); const x = 1; x |= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 8'); const x = 1; x ^= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 9'); const x = 1; x >>= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 10'); const x = 1; x <<= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 11'); const x = 1; x >>>= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 12'); const x = 1; x %= 2;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 13'); const x = 1; x ++;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 14'); const x = 1; x --;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 15'); const x = 1; ++ x;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 16'); const x = 1; -- x;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 17'); const x = 1; {x++;}"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 18'); const x = 1; {let x = 2; x++;}"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 19'); const x = 1; {x++; let x = 2;}"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 20'); const x = 1; {let x = 2;} x = 10"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 21'); const x = 1; {const x = 2; x++;}"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 22'); const x = 1; {const x = 2;} x++;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 23'); x = 1; {let x = 2;} const x = 10;"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }
try { eval("WScript.Echo('test 24'); function f() {x = 1; {let x = 2;} const x = 10;} f();"); WScript.Echo("passed"); } catch (e) { WScript.Echo(e); }


