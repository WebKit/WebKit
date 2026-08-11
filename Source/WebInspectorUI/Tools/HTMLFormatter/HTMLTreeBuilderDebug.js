HTMLTreeBuilderDebug = class HTMLTreeBuilderDebug
{
    // Public

    get debugText()
    {
        return this._lines.join("\n");
    }

    begin()
    {
        console.log("--- Begin ---");
        this._lines = [];
    }

    pushParserNode(node)
    {
        console.log(node);
        this._lines.push(JSON.stringify(node));
    }

    end()
    {
        console.log("--- End ---");
    }
}
