//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try { JSON.parse(''); } catch(e) { WScript.Echo(e); }
try { JSON.parse('--'); } catch(e) { WScript.Echo(e); }
try { JSON.parse('{"asdf  }'); } catch(e) { WScript.Echo(e); }
try { JSON.parse('{"asdf" }'); } catch(e) { WScript.Echo(e); }
try { JSON.parse('{"asdf":1'); } catch(e) { WScript.Echo(e); }
try { JSON.parse("[23"); } catch(e) { WScript.Echo(e); }
try { JSON.parse("[23,]"); } catch(e) { WScript.Echo(e); }
