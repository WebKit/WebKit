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
    return makeCrawlObject(eval(path), path);
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

function crawl(toCrawl) {
    while (toCrawl.length) {
        var crawlTarget = toCrawl.shift();
        var object = crawlTarget.value;
        var type = classNameForObject(object);
        // If we've already seen an object of this type, and it's not a collection, ignore it.
        if (resultsByType[type] && !object.item && !object.length)
            continue;

        resultsByType[type] = makeCrawlObject(object.isInner, crawlTarget.valuePath);
        pushPropertyValuesWithUnseenTypes(toCrawl, object, crawlTarget.valuePath);
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
htmlToAdd += "<form name='testForm'></form><form name='testForm'></form>";
inner.document.body.innerHTML = htmlToAdd;

var toCrawl = [
    evalToCrawlObject('inner.document.location'), // window.location is tested by other tests, so test document.location in this one.
    evalToCrawlObject('inner.document.forms.testForm'), // NamedNodesCollection has the wrong prototype, test that.
    evalToCrawlObject('inner'),
    // Can add more stuff to crawl here.
];
crawl(toCrawl);
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

document.body.removeChild(subframe);

var successfullyParsed = true;
