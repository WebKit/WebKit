description("Make sure prototypes are set up using the window a property came from, instead of the lexical global object.")

var subframe = document.createElement("iframe");
document.body.appendChild(subframe);
var inner = subframe.contentWindow; // Call it "inner" to make shouldBe output shorter

var skippedProperties = [
    // These reach outside the frame:
    "parent", "top", "opener", "frameElement",
    // These are defined by DumpRenderTree
    "GCController", "layoutTestController",
    "objCController", "textInputController", "navigationController",
    "eventSender", "objCPlugin", "objCPluginFunction",
    "appleScriptController", "plainText", "accessibilityController",
    // Skip our test property
    "isInner",
    // Ignore fooConstructor.prototype, fooInstance.__proto__ is more likely to fail.
    "prototype",
];

var skippedPropertiesSet = {};
for (var i = 0; i < skippedProperties.length; i++)
  skippedPropertiesSet[skippedProperties[i]] = true;

// Stash a property on the prototypes.
window.Object.prototype.isInner = false;
inner.Object.prototype.isInner = true;

function propertiesOnObject(object) {
    var properties = [];
    for (property in object) {
        if (skippedPropertiesSet[property])
            continue;
        properties.push(property);
    }
    return properties;
}

var resultsByType = {};

function classNameForObject(object)
{
    // call will use the global object if passed null or undefined, so special case those:
    if (object == null)
        return null;
    var result = Object.prototype.toString.call(object);
    // remove '[object ' and ']'
    return result.split(' ')[1].split(']')[0];
}

function constructorNamesForWindow(globalObject)
{
    var namesSet = {};
    for (var property in globalObject) {
        var value = inner[property];
        if (value == null)
            continue;
        var type = classNameForObject(value);
        // Ignore these properties because they do not exist in all implementations. They will be tested separately
        if (type == "WebGLRenderingContextConstructor" || 
            type == "WebGLArrayBufferConstructor" ||
            type == "WebGLByteArrayConstructor" ||
            type =="WebGLFloatArrayConstructor" ||
            type =="WebGLIntArrayConstructor" ||
            type =="WebGLShortArrayConstructor" ||
            type =="WebGLUnsignedByteArrayConstructor" ||
            type =="WebGLUnsignedIntArrayConstructor" ||
            type =="WebGLUnsignedShortArrayConstructor")
            continue; // We ignore WebGLRenderingContext and test it elsewhere, since it is not in all builds
        if (!type.match('Constructor$'))
            continue;
        namesSet[type] = 1;
    }
    return propertiesOnObject(namesSet).sort();
}

function makeCrawlObject(value, valuePath)
{
    return {
        'value' : value,
        'valuePath' : valuePath
    };
}

function evalToCrawlObject(path)
{
    // This allows us to add things to the end of the crawl list
    // without the early-eval changing the test results.
    function LazyEvalCrawlObject(path){
        this.valuePath = path;
        var value;

        this.__defineGetter__("value", function(){
            if (!value)
                value = eval(this.valuePath);
            return value;
        });
    }

    return new LazyEvalCrawlObject(path);
}

function pushPropertyValuesWithUnseenTypes(toCrawl, parentObject, parentPath)
{
    var propertiesToCrawl = propertiesOnObject(parentObject);
    propertiesToCrawl.push("__proto__");
    propertiesToCrawl.push("constructor");

    var parentType = classNameForObject(parentObject);
    
    for (var x = 0; x < propertiesToCrawl.length; x++) {
        var property = propertiesToCrawl[x];
        var value = parentObject[property];
        var valuePath = parentPath + "." + property;
        // CSSStyleDeclaration.item() just returns property names, but we want to crawl the actual CSS values
        if (parentType == "CSSStyleDeclaration" && parseInt(property)) { // This will skip 0, but that should be OK.
            valuePath = parentPath + ".getPropertyCSSValue(" + value + ")"
            value = parentObject.getPropertyCSSValue(value);
        }
        var type = classNameForObject(value);
        if (!type)
            continue;
         // We already have other tests which cover window.Foo constructor objects, so skip them.
         // fooInstance.constructor is the case we want to catch here.
        if (parentType == "DOMWindow" && type.match("Constructor$") && property != "constructor")
            continue;
        if (!resultsByType[type])
            toCrawl.push(makeCrawlObject(value, valuePath));
    }
}

function crawl(crawlStarts) {
    while (crawlStarts.length) {
        var toCrawl = [crawlStarts.shift()];
        while (toCrawl.length) {
            var crawlTarget = toCrawl.shift();
            var object = crawlTarget.value;
            var type = classNameForObject(object);
            // If we've already seen an object of this type, and it's not a collection
            if (resultsByType[type] && !object.item && !object.length) {
                // Make sure this isn't a new failure before skipping it.
                if (object.isInner || object.isInner === resultsByType[type].value)
                    continue;
            }

            resultsByType[type] = makeCrawlObject(object.isInner, crawlTarget.valuePath);
            pushPropertyValuesWithUnseenTypes(toCrawl, object, crawlTarget.valuePath);
        }
    }
}

var tagReplacements = {
    'anchor' : 'a',
    'dlist' : 'dl',
    'image' : 'img',
    'ulist' : 'ul',
    'olist' : 'ol',
    'tablerow' : 'tr',
    'tablecell' : 'td',
    'tablecol' : 'col',
    'tablecaption' : 'caption',
    'paragraph' : 'p',
    'blockquote' : 'quote',
}

function tagNameFromConstructor(constructorName)
{
    var expectedObjectName = constructorName.split("Constructor")[0];
    var match = expectedObjectName.match(/HTML(\w+)Element$/);
    if (!match)
        return null;
    var tagName = match[1].toLowerCase();
    if (tagReplacements[tagName])
        return tagReplacements[tagName];
    return tagName;
}


/* Give us more stuff to crawl */
/* If you're seeing "Never found..." errors from this test, more should be added here. */
var htmlToAdd = "";
var constructorNames = constructorNamesForWindow(inner);
for (var x = 0; x < constructorNames.length; x++) {
    var constructorName = constructorNames[x];
    var tagName = tagNameFromConstructor(constructorName);
    if (!tagName)
        continue;
    if (tagName == 'html')
        continue; // <html> causes a parse error.
    htmlToAdd += "<" + tagName + "></" + tagName + ">";
}
htmlToAdd += "<form name='testForm'><input name='testInput'></form><form name='testForm'></form>";
htmlToAdd += "<!-- test -->";
styleContents = "@charset 'UTF-8';";
styleContents += "@import url('dummy.css') print;\n"; // Our parser seems to want this rule first?
styleContents += "@variables { Ignored: 2em; }\n"; // For when variables are turned back on
styleContents += "@page { margin: 3cm; }\n"; // Current WebKit ignores @page
styleContents += "@media print { body { margin: 3cm; } }\n"
styleContents += "@font-face {font-family:'Times';}\n";
styleContents += "ignored {font-family: var(Ignored);}\n"; // a CSSStyleRule
styleContents += "@-webkit-keyframes fade { 0% { opacity: 0; } }\n"; // a WebKitCSSKeyframesRule

htmlToAdd += "<style id='dummyStyle'>" + styleContents + "</style>";
htmlToAdd += "<span id='styledSpan' style='clip: rect(0, 0, 1, 1); content: counter(dummy, square);'></span>";

inner.document.body.innerHTML = htmlToAdd;

var crawlStartPaths = [
    evalToCrawlObject('inner.document.location'), // window.location is tested by other tests, so test document.location in this one.
    //evalToCrawlObject('inner.testForm'), // Causes many failures
    evalToCrawlObject('inner.document.forms.testForm'), // NamedNodesCollection has the wrong prototype, test that.
    evalToCrawlObject('inner'),
    evalToCrawlObject('inner.document.testForm'),
    evalToCrawlObject('inner.document.testForm[0].testInput'),
    evalToCrawlObject('inner.document.getElementsByTagName("canvas")[0].getContext("2d")'), // for CanvasRenderingContext2D
    evalToCrawlObject('inner.document.getElementsByTagName("canvas")[0].getContext("2d").createPattern(inner.document.getElementsByTagName("img")[0], "")'), // for CanvasRenderingContext2D
    evalToCrawlObject('inner.document.body.getClientRects()'), // For ClientRectList
    evalToCrawlObject('inner.document.body.getBoundingClientRect()'), // For ClientRect, getClientRects() returns an empty list for in our test, not sure why.
    evalToCrawlObject('inner.getComputedStyle(inner.document.body)'),
    evalToCrawlObject('inner.document.getElementById("dummyStyle").sheet.cssRules'), // For various CSSRule subclasses
    evalToCrawlObject('inner.document.getElementById("styledSpan").style'),
    evalToCrawlObject('inner.document.getElementById("styledSpan").style.getPropertyCSSValue("clip").getRectValue()'),
    evalToCrawlObject('inner.document.getElementById("styledSpan").style.getPropertyCSSValue("content")[0].getCounterValue()'),
    // Can add more stuff to crawl here.
];

crawl(crawlStartPaths);
var sortedTypes = propertiesOnObject(resultsByType).sort();

// Run the actual tests
for (var x = 0; x < sortedTypes.length; x++) {
    var type = sortedTypes[x];
    var result = resultsByType[type];
    if (result.value)
        testPassed(type + " from " + result.valuePath);
    else
        testFailed(type + " from " + result.valuePath);
}

// Add a bunch of fails at the end for stuff we missed in our crawl.
for (var x = 0; x < constructorNames.length; x++) {
    var constructorName = constructorNames[x];
    var expectedObjectName = constructorName.split("Constructor")[0];
    if (expectedObjectName.match("Event$"))
        continue; // Not going to be able to test events with a crawl.
    if (expectedObjectName.match("Exception$"))
        continue; // Not going to be able to test exceptions with a crawl.
    if (expectedObjectName.match("Error$"))
        continue; // Not going to be able to test errors with a crawl.
    if (!resultsByType[expectedObjectName])
        debug("Never found " + expectedObjectName);
}

//document.body.removeChild(subframe);

var successfullyParsed = true;
