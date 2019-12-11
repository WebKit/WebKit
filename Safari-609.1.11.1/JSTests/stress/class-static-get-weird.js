{
    class c { static get name() { return 42; } }
    if (c.name !== 42) throw "Fail";
}

{
    class c { static get arguments() { return 42; } }
    if (c.arguments !== 42) throw "Fail";
}

{
    class c { static get caller() { return 42; } }
    if (c.caller !== 42) throw "Fail";
}

{
    class c { static get length() { return 42; } }
    if (c.length !== 42) throw "Fail";
}
