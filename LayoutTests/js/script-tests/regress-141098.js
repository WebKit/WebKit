description("Regression test for https://webkit.org/b/141098. Make sure eval() properly handles running out of stack space. This test should run without crashing.");

function probeAndRecurse(depth)
{
    var result;

    // Probe stack depth
    try {
        result = probeAndRecurse(depth+1);
        if (result < 0)
            return result + 1;
        else if (result > 0)
            return result;
    } catch (e) {
        // Go up a many frames and then create an expression to eval that will consume the stack using
        // callee registers.
        return -60;
    }

    try {
        var count = 1;

        for (var i = 0; i < 40; count *= 10, i++) {
            evalStringPrefix = "{ var first = " + count + "; ";
            var evalStringBody = "";

            for (var varIndex = 0; varIndex < count; varIndex++)
                evalStringBody += "var s" + varIndex + " = " + varIndex + ";";

            evalStringBody += "var value = [";
            for (var varIndex = 0; varIndex < count; varIndex++) {
                if (varIndex > 0)
                    evalStringBody += ", ";
                evalStringBody += "s" + varIndex;
            }
            evalStringBody +=  "]; ";

           var evalResult = eval("{" + evalStringBody + "}");
        }
    } catch (e) {
    }

    return 1;
}

probeAndRecurse(0);
