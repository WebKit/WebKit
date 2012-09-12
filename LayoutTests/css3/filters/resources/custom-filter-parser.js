// This is a helper script designed to parse the computed syntax of the custom filter. 
// Note that it is generic enough so that it can be used for other properties in the future.

function Token(type) 
{
    this.type = type;
}

// Checks if the type of the token is in the list of arguments passed to the function.
// It also accepts arrays and other type checking functions.
Token.prototype.isA = function()
{
    for (var i = 0; i < arguments.length; ++i) {
        var type = arguments[i];
        if ((typeof type == "object" && type.length && this.isA.apply(this, type))
            || (typeof type == "function" && type(this))
            || type == this.type)
            return true;
    }
    return false;
}

// Creates a new token object and copies all the properties in "value" to the new token.
function createToken(type, value) {
    var token = new Token(type);
    if (value) {
        for (var i in value)
            if (value.hasOwnProperty(i))
                token[i] = value[i];
    }
    return token;
}

// Tokenizes the string into Tokens of types [urls, floats, integers, keywords, ",", "(", ")" and "."].
function tokenizeString(s)
{
    var tokenizer = new RegExp([
        "url\\(\\s*(.*?)\\s*\\)", // url() - 1
        "((?:[\\+-]?)\\d*\\.\\d+)", // floats - 2
        "((?:[\\+-]?)\\d+)", // integers - 3
        "([\\w-][\\w\\d-]*)", // keywords - 4
        "([,\\.\\(\\)])" // punctuation - 5
    ].join("|"), "g");

    var match, tokens = [];
    while (match = tokenizer.exec(s)) {
        if (match[1] !== undefined)
            tokens.push(createToken("url", {value: match[1]}));
        else if (match[2] !== undefined)
            tokens.push(createToken("float", {value: parseFloat(match[2])}));
        else if (match[3] !== undefined)
            tokens.push(createToken("integer", {value: parseInt(match[3])}));
        else if (match[4] !== undefined)
            tokens.push(createToken("keyword", {value: match[4]}));
        else if (match[5] !== undefined)
            tokens.push(createToken(match[5]));
    }
    return tokens;
}

// Checks if the token is a number. Can be used in combination with the "Token.isA" method.
function number(token)
{
    return token.type == "float" || token.type == "integer";
}

// Helper class to iterate on the token stream. It will add an "end"
// token at the end to make it easier to use the "ahead" function
// without checking if it's the last token in the stream.
function TokenStream(tokens) {
    this.tokens = tokens;
    this.tokens.push(new Token("end"));
    this.tokenIndex = 0;
}

TokenStream.prototype.current = function() 
{ 
    return this.tokens[this.tokenIndex]; 
}

TokenStream.prototype.ahead = function() 
{
    return this.tokens[this.tokenIndex + 1]; 
}

// Skips the current token only if it matches the "typeMatcher". Otherwise it throws an error.
TokenStream.prototype.skip = function(typeMatcher)
{
    if (!typeMatcher || this.current().isA(typeMatcher)) {
        var token = this.current();
        ++this.tokenIndex;
        return token;
    }
    throw new Error("Cannot use " + JSON.stringify(typeMatcher) + " to skip over " + JSON.stringify(this.ahead()));
}

// Skips the current token, only if it matches the "typeMatcher". Makes it easy to skip over comma tokens.
TokenStream.prototype.skipIfNeeded = function(typeMatcher)
{
    if (this.ahead() && this.ahead().isA(typeMatcher))
        ++this.tokenIndex;
}

// Creates a new "function" node. It expects that the current token is the function name.
function parseFunction(m)
{
    var functionObject = {
        type: "function",
        name: m.skip().value
    };
    m.skip("(");
    functionObject.arguments = parseList(m, [")", "end"]);
    m.skip(")");
    return functionObject;
}

// Creates a new "parameter" node. It expects that the current token is the parameter name.
// It consumes everything before the following comma.
function parseParameter(m)
{
    var functionObject = {
        type: "parameter",
        name: m.skip().value
    };
    functionObject.value = parseList(m, [",", "end"]);
    m.skipIfNeeded(",");
    return functionObject;
}

// Consumes a list of tokens before reaching the "endToken" and returns an array with all the parsed items.
// Makes the following assumptions:
// - if a keyword is followed by "(" then it is a start of function
// - if a keyword is not followed by "," it is a parameter
// Keywords that do not match either of the previous rules and tokens like number and url are just cloned.
function parseList(m, endToken)
{
    var result = [], token;
    while ((token = m.current()) && !token.isA(endToken)) {
        if (token.isA("keyword")) {
            if (m.ahead().isA("("))
                result.push(parseFunction(m));
            else if (m.ahead().isA(",", endToken)) {
                result.push({
                    type: "keyword", 
                    value: m.skip().value
                });
            } else
                result.push(parseParameter(m));
        } else if (token.isA(number)) {
            result.push({
                type: "number",
                value: m.skip().value
            });
        } else if (token.isA("url")) {
            result.push({
                type: "url",
                value: m.skip().value
            });
        } else if (token.isA(","))
            m.skip();
        else 
            throw "Unexpected token " + JSON.stringify(token) + " in a list.";
    }
    return result;
}

function tokensToValues(tokens)
{
    var m = new TokenStream(tokens);
    return parseList(m, "end");
}

// Extracts a parameters array from the parameters string of the custom filter function.
function parseCustomFilterParameters(s)
{
    return tokensToValues(tokenizeString(s));
}

// Need to remove the base URL to avoid having local paths in the expected results.
function removeBaseURL(src) {
    var urlRegexp = /url\(([^\)]*)\)/g;
    return src.replace(urlRegexp, function(match, url) {
        return "url(" + url.substr(url.lastIndexOf("/") + 1) + ")";
    });
}

// Parses the parameters of the custom filter function and returns it using the following order:
// - If parameters have different types (ie. url, number, named parameter), the alphabetical order of the "type" is used.
// - If parameters are of type "parameter" the alphabetical order of the parameter names is used.
// - If parameters are of other types, then the value is used to order them alphabetical.
// The order is important to make it easy to compare two custom filters, that have exactly the same parameters, 
// but with potentially different "named parameter" values.
function getCustomFilterParameters(s) 
{
    return parseCustomFilterParameters(removeBaseURL(s)).sort(function(a, b) { 
        if (a.type != b.type) 
            return a.type.localeCompare(b.type);
        if (a.type == "parameter")
            return a.name.localeCompare(b.name);
        return a.value.toString().localeCompare(b.value.toString());
    });
}
