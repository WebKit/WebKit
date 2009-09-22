description('This test makes sure stack unwinding works correctly when it occurs inside an eval contained in a dynamic scope.');
var result;
function runTest() {
    var test = "outer scope";
    with({test:"inner scope"}) {
        eval("try { throw ''; } catch (e) { result = test; shouldBe('result', '\"inner scope\"'); }");
        result = null;
        eval("try { with({test:'innermost scope'}) throw ''; } catch (e) { result = test; shouldBe('result', '\"inner scope\"'); }");
        result = null;
        eval("with ({test:'innermost scope'}) try { throw ''; } catch (e) { result = test; shouldBe('result', '\"innermost scope\"'); }");
        result = null;
        with ({test:'innermost scope'}) eval("try { throw ''; } catch (e) { result = test; shouldBe('result', '\"innermost scope\"'); }");
        result = null;
        try {
            eval("try { throw ''; } finally { result = test; shouldBe('result', '\"inner scope\"'); result = null; undeclared; }");
        } catch(e) {
            result = test;
            shouldBe('result', '"inner scope"');
            result = null;
            eval("try { with({test:'innermost scope'}) throw ''; } catch (e) { result = test; shouldBe('result', '\"inner scope\"'); }");
            result = null;
            eval("with ({test:'innermost scope'}) try { throw ''; } catch (e) { result = test; shouldBe('result', '\"innermost scope\"'); }");
            result = null;
            with ({test:'innermost scope'}) eval("try { throw ''; } catch (e) { result = test; shouldBe('result', '\"innermost scope\"'); }");
        }
    }
    result = test;
    eval("try { throw ''; } catch (e) { result = test; shouldBe('result', '\"outer scope\"'); }");
    eval("result = test");
    eval("try { throw ''; } catch (e) { result = test; shouldBe('result', '\"outer scope\"'); }");
}
runTest();

var successfullyParsed = true;
