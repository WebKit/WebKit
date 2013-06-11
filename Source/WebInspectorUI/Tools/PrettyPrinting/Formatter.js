function Formatter(codeMirror, builder)
{
    console.assert(codeMirror);
    console.assert(builder);

    this._codeMirror = codeMirror;
    this._builder = builder;

    this._lastToken = null;
    this._lastContent = "";
};

Formatter.prototype = {
    constructor: Formatter,

    // Public

    format: function(from, to)
    {
        console.assert(this._builder.originalContent === null);
        if (this._builder.originalContent !== null)
            return;

        var outerMode = this._codeMirror.getMode();
        var content = this._codeMirror.getRange(from, to);
        var state = CodeMirror.copyState(outerMode, this._codeMirror.getTokenAt(from).state);
        this._builder.setOriginalContent(content);

        var lineOffset = 0;
        var lines = content.split("\n");
        for (var i = 0; i < lines.length; ++i) {
            var line = lines[i];
            var startOfNewLine = true;
            var firstTokenOnLine = true;
            var stream = new CodeMirror.StringStream(line);
            while (!stream.eol()) {
                var innerMode = CodeMirror.innerMode(outerMode, state);
                var token = outerMode.token(stream, state);
                var isWhiteSpace = token === null && /^\s*$/.test(stream.current());
                this._handleToken(innerMode.mode, token, state, stream, lineOffset + stream.start, isWhiteSpace, startOfNewLine, firstTokenOnLine);
                stream.start = stream.pos;
                startOfNewLine = false;
                if (firstTokenOnLine && !isWhiteSpace)
                    firstTokenOnLine = false;
            }

            if (firstTokenOnLine)
                this._handleEmptyLine();

            lineOffset += line.length + 1; // +1 for the "\n" removed in split.
            this._handleLineEnding(lineOffset - 1); // -1 for the index of the "\n".
        }

        this._builder.finish();
    },

    // Private

    _handleToken: function(mode, token, state, stream, originalPosition, isWhiteSpace, startOfNewLine, firstTokenOnLine)
    {
        // String content of the token.
        var content = stream.current();

        // Start of a new line. Insert a newline to be safe if code was not-ASI safe. These are collapsed.
        if (startOfNewLine)
            this._builder.appendNewline();

        // Whitespace. Collapse to a single space.
        if (isWhiteSpace) {
            this._builder.appendSpace();
            return;
        }

        // Avoid some hooks for content in comments.
        var isComment = token && /\bcomment\b/.test(token);

        if (mode.modifyStateForTokenPre)
            mode.modifyStateForTokenPre(this._lastToken, this._lastContent, token, state, content, isComment);

        // Should we remove the last newline?
        if (this._builder.lastTokenWasNewline && mode.removeLastNewline(this._lastToken, this._lastContent, token, state, content, isComment, firstTokenOnLine))
            this._builder.removeLastNewline();

        // Add whitespace after the last token?
        if (!this._builder.lastTokenWasWhitespace && mode.shouldHaveSpaceAfterLastToken(this._lastToken, this._lastContent, token, state, content, isComment))
            this._builder.appendSpace();

        // Add whitespace before this token?
        if (!this._builder.lastTokenWasWhitespace && mode.shouldHaveSpaceBeforeToken(this._lastToken, this._lastContent, token, state, content, isComment))
            this._builder.appendSpace();

        // Should we dedent before this token?
        var dedents = mode.dedentsBeforeToken(this._lastToken, this._lastContent, token, state, content, isComment);
        while (dedents-- > 0)
            this._builder.dedent();

        // Should we add a newline before this token?
        if (mode.newlineBeforeToken(this._lastToken, this._lastContent, token, state, content, isComment))
            this._builder.appendNewline();

        // Should we indent before this token?
        if (mode.indentBeforeToken(this._lastToken, this._lastContent, token, state, content, isComment))
            this._builder.indent();

        // Append token.
        this._builder.appendToken(content, originalPosition);

        // Let the pretty printer update any state it keeps track of.
        if (mode.modifyStateForTokenPost)
            mode.modifyStateForTokenPost(this._lastToken, this._lastContent, token, state, content, isComment);

        // Should we indent or dedent after this token?
        if (!isComment && mode.indentAfterToken(this._lastToken, this._lastContent, token, state, content, isComment))
            this._builder.indent();

        // Should we add newlines after this token?
        var newlines = mode.newlinesAfterToken(this._lastToken, this._lastContent, token, state, content, isComment);
        if (newlines)
            this._builder.appendMultipleNewlines(newlines);

        // Record this token as the last token.
        this._lastToken = token;
        this._lastContent = content;
    },

    _handleEmptyLine: function()
    {
        // Preserve original whitespace only lines by adding a newline.
        // However, don't do this if the builder just added multiple newlines.
        if (!(this._builder.lastTokenWasNewline && this._builder.lastNewlineAppendWasMultiple))
            this._builder.appendNewline(true);
    },

    _handleLineEnding: function(originalNewLinePosition)
    {
        // Record the original line ending.
        this._builder.addOriginalLineEnding(originalNewLinePosition);
    }
};
