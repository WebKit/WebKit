EsprimaFormatterDebug = class EsprimaFormatterDebug
{
    constructor(sourceText, sourceType)
    {
        let tree = esprima.parse(sourceText, {attachComment: true, range: true, tokens: true, sourceType});
        let walker = new ESTreeWalker(this._before.bind(this), this._after.bind(this));

        this._statistics = {
            nodeTypes: {},
            tokenTypes: {},
        };

        this._nextCommentId = 1;

        this._lines = [];
        this._tokens = tree.tokens;
        this._tokensLength = this._tokens.length;
        this._tokenIndex = 0;
        this._debugHeader();
        walker.walk(tree);
        this._debugAfterProgramNode(tree);
        this._debugFooter();
    }

    // Public

    get debugText()
    {
        return this._lines.join("\n");
    }

    // Private

    _pad(str, width)
    {
        let result = str;
        for (let toPad = width - result.length; toPad > 0; --toPad)
            result += " ";
        return result;
    }

    _debugHeader()
    {
        let nodeString = this._pad("Node Type", 25);
        let tokenTypeString = this._pad("Token Type", 20);
        let tokenValueString = "Token Value";
        this._lines.push(nodeString + " " + tokenTypeString + " " + tokenValueString);
        this._lines.push("-".repeat(25) + " " + "-".repeat(20) + " " + "-".repeat(20));
    }

    _debugFooter()
    {
        this._lines.push("");
        this._lines.push(this._pad("Node Type", 25) + " Frequency");
        this._lines.push("-".repeat(25) + " " + "-".repeat(15));
        let groups = [];
        for (let key of Object.keys(this._statistics.nodeTypes))
            groups.push([key, this._statistics.nodeTypes[key]]);
        groups.sort((a, b) => b[1] - a[1]);
        for (let [nodeType, frequency] of groups)
            this._lines.push(this._pad(nodeType, 25) + " " + frequency);

        this._lines.push("");
        this._lines.push(this._pad("Token Type", 20) + " Frequency");
        this._lines.push("-".repeat(20) + " " + "-".repeat(15));
        groups = [];
        for (let key of Object.keys(this._statistics.tokenTypes))
            groups.push([key, this._statistics.tokenTypes[key]]);
        groups.sort((a, b) => b[1] - a[1]);
        for (let [tokenType, frequency] of groups)
            this._lines.push(this._pad(tokenType, 20) + " " + frequency);
    }

    _debug(node, token)
    {
        let nodeString = this._pad(node.type, 25);
        let tokenTypeString = this._pad(token.type, 20);
        let tokenValueString = token.value;
        this._lines.push(nodeString + " " + tokenTypeString + " " + tokenValueString);

        if (!this._statistics.nodeTypes[node.type])
            this._statistics.nodeTypes[node.type] = 0;
        if (!this._statistics.tokenTypes[token.type])
            this._statistics.tokenTypes[token.type] = 0;
        this._statistics.nodeTypes[node.type]++;
        this._statistics.tokenTypes[token.type]++;
    }

    _debugComments(comments, trailing)
    {
        for (let comment of comments) {
            if (!comment.__id)
                comment.__id = this._nextCommentId++;
            let nodeString = this._pad(`** ${trailing ? "T " :"L "}Comment (${comment.__id})`, 25);
            let commentTypeString = this._pad(comment.type, 20);
            let commentValueString = comment.value;
            this._lines.push(nodeString + " " + commentTypeString + " " + commentValueString);
        }
    }

    _debugAfterProgramNode(programNode)
    {
        if (programNode.body.length) {
            let lastNode = programNode.body[programNode.body.length - 1];
            if (lastNode.trailingComments)
                this._debugComments(lastNode.trailingComments, true);
        } else {
            if (programNode.leadingComments)
                this._debugComments(programNode.leadingComments);
            if (programNode.trailingComments)
                this._debugComments(programNode.trailingComments, true);
        }
    }

    _before(node)
    {
        if (!node.parent)
            return;

        while (this._tokenIndex < this._tokensLength && this._tokens[this._tokenIndex].range[0] < node.range[0]) {
            let token = this._tokens[this._tokenIndex++];
            this._debug(node.parent, token);
        }

        if (node.leadingComments)
            this._debugComments(node.leadingComments);
    }

    _after(node)
    {
        while (this._tokenIndex < this._tokensLength && this._tokens[this._tokenIndex].range[0] < node.range[1]) {
            let token = this._tokens[this._tokenIndex++];
            this._debug(node, token);
        }

        if (node.trailingComments)
            this._debugComments(node.trailingComments, true);
    }
}
