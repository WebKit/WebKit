description('This test verifies the result returned by the eval function when exceptions are thrown and caught whithin the contents of the evaluated string.');

function throwFunc() {
  throw "";
}

function throwOnReturn(){
  1;
  return throwFunc();
} 

function twoFunc() {
  2;
}

shouldBe('eval("1;")', "1");
shouldBe('eval("1; try { foo = [2,3,throwFunc(), 4]; } catch (e){}")', "1");
shouldBe('eval("1; try { 2; throw \\"\\"; } catch (e){}")', "2");
shouldBe('eval("1; try { 2; throwFunc(); } catch (e){}")', "2");
shouldBe('eval("1; try { 2; throwFunc(); } catch (e){3;} finally {}")', "3");
shouldBe('eval("1; try { 2; throwFunc(); } catch (e){3;} finally {4;}")', "4");
shouldBe('eval("function blah() { 1; }\\n blah();")', "undefined");
shouldBe('eval("var x = 1;")', "undefined");
shouldBe('eval("if (true) { 1; } else { 2; }")', "1");
shouldBe('eval("if (false) { 1; } else { 2; }")', "2");
shouldBe('eval("try{1; if (true) { 2; throw \\"\\"; } else { 2; }} catch(e){}")', "2");
shouldBe('eval("1; var i = 0; do { ++i; 2; } while(i!=1);")', "2");
shouldBe('eval("try{1; var i = 0; do { ++i; 2; throw \\"\\"; } while(i!=1);} catch(e){}")', "2");
shouldBe('eval("1; try{2; throwOnReturn();} catch(e){}")', "2");
shouldBe('eval("1; twoFunc();")', "undefined");
shouldBe('eval("1; with ( { a: 0 } ) { 2; }")', "2");

var successfullyParsed = true;
