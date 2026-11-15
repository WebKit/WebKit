function mustThrowSyntaxError(source) {
    let threw = false;
    try {
        new Function(source);
    } catch (error) {
        if (!(error instanceof SyntaxError))
            throw new Error(String(error));
        threw = true;
    }
    if (!threw)
        throw new Error("parsed " + source);
}

mustThrowSyntaxError("x" + "[".repeat(40));
mustThrowSyntaxError("x(" + "[".repeat(40));
mustThrowSyntaxError("x = " + "[".repeat(40));
mustThrowSyntaxError("x = " + "{a:".repeat(40));

function mustParse(source) {
    try {
        new Function(source);
    } catch (error) {
        throw new Error(source + " -> " + error);
    }
}

mustParse("let [[a] = [1]] = [[2]]");
mustParse("[[a] = [1]] = [[2]]");
mustParse("let {a: {b} = {b: 1}} = {a: {}}");
mustParse("let o = {}; [[o.x] = [1]] = [[2]]");
mustParse("let a = 1; [[a + 1].b] = [7]");
mustParse("[{a: 1}.b] = [8]");
mustParse("let o = {}; [[o][0]] = [9]");
mustParse("[[].length] = [3]");
mustParse("let a = 1; [[(a + 1)].b] = [6]");
mustParse("[{a: function(){ return 1 }}.a] = [11]");
mustParse("[[/]/].length] = [12]");
mustParse("[{a: `x`}.a] = [13]");
mustParse("let a = 1; [[a + 1] /*c*/ .b] = [14]");
mustParse("let a = 1; [[a + 1]\n.b] = [15]");
mustParse("let [a] = [1]");
mustParse("let [...a] = [1, 2]");
mustParse("let x = ([a]) => a");
mustParse("([a] = [1]) => a");
mustParse("'use strict'; let a = 1; [[a + 1].b] = [7]");
mustThrowSyntaxError("([[a + 1].b] = [1]) => 0");
mustParse("let [a = [1, 2][0]] = []");
mustParse("let o = {}; [[o.a].b] = [{b: 1}]");
mustParse("let a; [[[a] = [1], 2].length] = [9]");
mustParse("[{a: /\\[\\[/}.b] = [0]");
mustParse("[...[{prop: 1}.prop]] = []");

mustThrowSyntaxError("[[]()] = [1]");
mustThrowSyntaxError("[[a + 1]] = [1]");
mustThrowSyntaxError("[this] = []");
mustThrowSyntaxError("({foo() {}} = {})");
