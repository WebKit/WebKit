function library()
{
}

for (var name of ["plugin", "stringify", "parse", "fromJSON", "list", "comment", "atRule", "decl", "rule", "root", "document", "CssSyntaxError", "Declaration", "Container", "Processor", "Document", "Comment", "Warning", "AtRule", "Result", "Input", "Rule", "Root", "Node", "default"])
    library[name] = function () { };

function test(result)
{
    return { ...library, library, result };
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(i);
