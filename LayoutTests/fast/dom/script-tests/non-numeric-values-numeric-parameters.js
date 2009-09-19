description(
'This tests the behavior of non-numeric values in contexts where the DOM has a numeric parameter.'
);

function nonNumericPolicy(template)
{
    var x = 0;
    try {
        eval(template);
    } catch (e) {
        return e;
    }

    var nullAllowed = 1;
    x = null;
    try {
        eval(template);
    } catch (e) {
        nullAllowed = 0;
    }

    var undefinedAllowed = 1;
    x = undefined;
    try {
        eval(template);
    } catch (e) {
        undefinedAllowed = 0;
    }

    var stringAllowed = 1;
    x = "string";
    try {
        eval(template);
    } catch (e) {
        stringAllowed = 0;
    }

    var documentAllowed = 1;
    x = document;
    try {
        eval(template);
    } catch (e) {
        documentAllowed = 0;
    }

    var nonIntegerAllowed = 1;
    x = 0.1;
    try {
        eval(template);
    } catch (e) {
        nonIntegerAllowed = 0;
    }

    var infinityAllowed = 1;
    x = Infinity;
    try {
        eval(template);
    } catch (e) {
        infinityAllowed = 0;
    }

    var nanAllowed = 1;
    x = NaN;
    try {
        eval(template);
    } catch (e) {
        NaNAllowed = 0;
    }

    var omitAllowed = -1; // means "not applicable"
    var templateWithoutArg = template.replace(", x)", ")").replace("(x)", "()");
    if (templateWithoutArg != template) {
        omitAllowed = 1;
        try {
            eval(templateWithoutArg);
        } catch(e) {
            omitAllowed = 0;
        }
    }

    var expectOmitAllowed = navigator.userAgent.match("Gecko/") != "Gecko/";

    if (nullAllowed && undefinedAllowed && stringAllowed && documentAllowed && nonIntegerAllowed && infinityAllowed && nanAllowed) {
        if (omitAllowed == -1 || omitAllowed == (expectOmitAllowed ? 1 : 0))
            return "any type allowed";
        if (omitAllowed == 1)
            return "any type allowed (or omitted)";
        if (omitAllowed == 0)
            return "any type allowed (but not omitted)";
    }
    if (nullAllowed && !undefinedAllowed && !stringAllowed && !documentAllowed && nonIntegerAllowed && !infinityAllowed && nanAllowed && omitAllowed == 1)
        return "number or null allowed (or omitted, but not infinite)";
    return "mixed";
}

var selector = "a";
var styleText = "font-size: smaller";
var ruleText = selector + " { " + styleText + " }";

var testElementContainer = document.createElement("div");
document.body.appendChild(testElementContainer);

function createCSSStyleSheet()
{
    return document.createElement("style").sheet;
}

function createFromMarkup(markup)
{
    var range = document.createRange();
    var fragmentContainer = document.createElement("div");
    range.selectNodeContents(fragmentContainer);
    testElementContainer.appendChild(fragmentContainer);
    var fragment = range.createContextualFragment(markup);
    fragmentContainer.appendChild(fragment);
    return fragmentContainer.firstChild;
}

function createCSSStyleSheet()
{
    return createFromMarkup("<style>" + ruleText + "</style>").sheet;
}

function createCSSRuleList()
{
    return createCSSStyleSheet().cssRules;
}

function createCSSStyleDeclaration()
{
    return createCSSRuleList().item(0).style;
}

function createCSSMediaRule()
{
    var rule = createFromMarkup("<style>@media screen { a { text-weight: bold } }</style>").sheet.cssRules.item(0);
    rule.insertRule(ruleText, 0);
    return rule;
}

function createCSSValueList()
{
    // FIXME: Not working in Firefox, not sure why.
    return createFromMarkup("<style>a { font-family: b, c }</style>").sheet.cssRules.item(0).style.getPropertyCSSValue("font-family");
}

function createMediaList()
{
    return createCSSMediaRule().media;
}

function createHTMLSelectElement()
{
    var select = document.createElement("select");
    select.options.add(document.createElement("option"));
    return select;
}

function createHTMLOptionsCollection()
{
    return createHTMLSelectElement().options;
}

function createHTMLTableElement()
{
    var table = document.createElement("table");
    table.insertRow(0);
    return table;
}

function createHTMLTableSectionElement()
{
    var table = document.createElement("table");
    table.insertRow(0);
    return table.tBodies[0];
}

function createHTMLTableRowElement()
{
    var table = document.createElement("table");
    var row = table.insertRow(0);
    row.insertCell(0);
    return row;
}

// CharacterData

shouldBe("nonNumericPolicy('document.createTextNode(\"a\").substringData(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createTextNode(\"a\").substringData(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createTextNode(\"a\").insertData(x, \"b\")')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createTextNode(\"a\").deleteData(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createTextNode(\"a\").deleteData(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createTextNode(\"a\").replaceData(x, 0, \"b\")')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createTextNode(\"a\").replaceData(0, x, \"b\")')", "'any type allowed'");

// CSSMediaRule

shouldBe("nonNumericPolicy('createCSSMediaRule().insertRule(ruleText, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createCSSMediaRule().deleteRule(x)')", "'any type allowed'");

// CSSRuleList

shouldBe("nonNumericPolicy('createCSSRuleList().item(x)')", "'any type allowed'");

// CSSStyleDeclaration

shouldBe("nonNumericPolicy('createCSSStyleDeclaration().item(x)')", "'any type allowed'");

// CSSStyleSheet

shouldBe("nonNumericPolicy('createCSSStyleSheet().insertRule(ruleText, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createCSSStyleSheet().deleteRule(x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createCSSStyleSheet().addRule(selector, styleText, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createCSSStyleSheet().removeRule(x)')", "'any type allowed'");

// CSSValueList

shouldBe("nonNumericPolicy('createCSSValueList().item(x)')", "'any type allowed'");

// Document

shouldBe("nonNumericPolicy('document.elementFromPoint(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.elementFromPoint(0, x)')", "'any type allowed'");

// Element

shouldBe("nonNumericPolicy('document.body.scrollByLines(x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.body.scrollByPages(x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.body.scrollLeft = x')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.body.scrollTop = x')", "'any type allowed'");

// History

// Not tested: go.

// HTMLCollection

shouldBe("nonNumericPolicy('document.images.item(x)')", "'any type allowed'");

// HTMLInputElement

shouldBe("nonNumericPolicy('document.createElement(\"input\").setSelectionRange(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createElement(\"input\").setSelectionRange(0, x)')", "'any type allowed'");

// HTMLOptionsCollection

shouldBe("nonNumericPolicy('createHTMLOptionsCollection().add(document.createElement(\"option\"), x)')", "'number or null allowed (or omitted, but not infinite)'");
shouldBe("nonNumericPolicy('createHTMLOptionsCollection().remove(x)')", "'any type allowed'");

// HTMLSelectElement

shouldBe("nonNumericPolicy('createHTMLSelectElement().remove(x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createHTMLSelectElement().item(x)')", "'any type allowed'");

// HTMLTableElement

shouldBe("nonNumericPolicy('createHTMLTableElement().insertRow(x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createHTMLTableElement().deleteRow(x)')", "'any type allowed'");

// HTMLTableRowElement

shouldBe("nonNumericPolicy('createHTMLTableRowElement().insertCell(x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createHTMLTableRowElement().deleteCell(x)')", "'any type allowed'");

// HTMLTableSectionElement

shouldBe("nonNumericPolicy('createHTMLTableSectionElement().insertRow(x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('createHTMLTableSectionElement().deleteRow(x)')", "'any type allowed'");

// HTMLInputElement

shouldBe("nonNumericPolicy('document.createElement(\"textarea\").setSelectionRange(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createElement(\"textarea\").setSelectionRange(0, x)')", "'any type allowed'");

// KeyboardEvent

shouldBe("nonNumericPolicy('document.createEvent(\"KeyboardEvent\").initKeyboardEvent(\"a\", false, false, null, \"b\", x, false, false, false, false, false)')", "'any type allowed'");

// MediaList

shouldBe("nonNumericPolicy('createMediaList().item(x)')", "'any type allowed'");

// MouseEvent

shouldBe("nonNumericPolicy('document.createEvent(\"MouseEvent\").initMouseEvent(\"a\", false, false, null, x, 0, 0, 0, 0, false, false, false, false, 0, null)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createEvent(\"MouseEvent\").initMouseEvent(\"a\", false, false, null, 0, x, 0, 0, 0, false, false, false, false, 0, null)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createEvent(\"MouseEvent\").initMouseEvent(\"a\", false, false, null, 0, 0, x, 0, 0, false, false, false, false, 0, null)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createEvent(\"MouseEvent\").initMouseEvent(\"a\", false, false, null, 0, 0, 0, x, 0, false, false, false, false, 0, null)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createEvent(\"MouseEvent\").initMouseEvent(\"a\", false, false, null, 0, 0, 0, 0, x, false, false, false, false, 0, null)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createEvent(\"MouseEvent\").initMouseEvent(\"a\", false, false, null, 0, 0, 0, 0, 0, false, false, false, false, x, null)')", "'any type allowed'");

// NamedNodeMap

shouldBe("nonNumericPolicy('document.body.attributes.item(x)')", "'any type allowed'");

// NodeIterator

shouldBe("nonNumericPolicy('document.createNodeIterator(document, x, null, false)')", "'any type allowed'");

// NodeList

shouldBe("nonNumericPolicy('document.getElementsByTagName(\"div\").item(x)')", "'any type allowed'");

// ProgressEvent

shouldBe("nonNumericPolicy('document.createEvent(\"ProgressEvent\").initProgressEvent(\"a\", false, false, false, x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createEvent(\"ProgressEvent\").initProgressEvent(\"a\", false, false, false, 0, x)')", "'any type allowed'");

// Range

shouldBe("nonNumericPolicy('document.createRange().setStart(document, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createRange().setEnd(document, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createRange().comparePoint(document, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('document.createRange().isPointInRange(document, x)')", "'any type allowed'");

// Selection

shouldBe("nonNumericPolicy('getSelection().collapse(document, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('getSelection().setBaseAndExtent(document, x, document, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('getSelection().setBaseAndExtent(document, 0, document, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('getSelection().setPosition(document, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('getSelection().extend(document, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('getSelection().getRangeAt(x)')", "'any type allowed'");

// SQLResultSetRowList

// Not tested: item.

// StyleSheetList

shouldBe("nonNumericPolicy('document.styleSheets.item(x)')", "'any type allowed'");

// Text

shouldBe("nonNumericPolicy('document.createTextNode(\"a\").splitText(x)')", "'any type allowed'");

// TimeRanges

// Not tested: start, end.

// TreeWalker

shouldBe("nonNumericPolicy('document.createTreeWalker(document, x, null, false)')", "'any type allowed'");

// UIEvent

shouldBe("nonNumericPolicy('document.createEvent(\"UIEvent\").initUIEvent(\"a\", false, false, null, x)')", "'any type allowed'");

// Window

shouldBe("nonNumericPolicy('window.scrollBy(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.scrollBy(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.scrollTo(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.scrollTo(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.scroll(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.scroll(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.moveBy(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.moveBy(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.moveTo(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.moveTo(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.resizeBy(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.resizeBy(0, x)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.resizeTo(x, 0)')", "'any type allowed'");
shouldBe("nonNumericPolicy('window.resizeTo(0, x)')", "'any type allowed'");
// Not tested: openDatabase.

window.resizeTo(10000, 10000);
document.body.removeChild(testElementContainer);

var successfullyParsed = true;

/*

Here are other examples of numeric types in function parameters and settable attributes that we could test:

../../../../WebCore/css/CSSPrimitiveValue.idl:                                          in float floatValue)
../../../../WebCore/html/CanvasGradient.idl:        void addColorStop(in float offset, in DOMString color);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void scale(in float sx, in float sy);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void rotate(in float angle);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void translate(in float tx, in float ty);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        CanvasGradient createLinearGradient(in float x0, in float y0, in float x1, in float y1);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        CanvasGradient createRadialGradient(in float x0, in float y0, in float r0, in float x1, in float y1, in float r1);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void clearRect(in float x, in float y, in float width, in float height)
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void fillRect(in float x, in float y, in float width, in float height)
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void moveTo(in float x, in float y);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void lineTo(in float x, in float y);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void quadraticCurveTo(in float cpx, in float cpy, in float x, in float y);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void bezierCurveTo(in float cp1x, in float cp1y, in float cp2x, in float cp2y, in float x, in float y);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void arcTo(in float x1, in float y1, in float x2, in float y2, in float radius)
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void rect(in float x, in float y, in float width, in float height)
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void arc(in float x, in float y, in float radius, in float startAngle, in float endAngle, in boolean anticlockwise)
../../../../WebCore/html/CanvasRenderingContext2D.idl:        boolean isPointInPath(in float x, in float y);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void setAlpha(in float alpha);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void setLineWidth(in float width);
../../../../WebCore/html/CanvasRenderingContext2D.idl:        void setMiterLimit(in float limit);

../../../../WebCore/html/HTMLAnchorElement.idl:        attribute long tabIndex;
../../../../WebCore/html/HTMLAppletElement.idl:                 attribute [ConvertFromString] long hspace;
../../../../WebCore/html/HTMLAppletElement.idl:                 attribute [ConvertFromString] long vspace;
../../../../WebCore/html/HTMLAreaElement.idl:        attribute long tabIndex;
../../../../WebCore/html/HTMLBaseFontElement.idl:        attribute long size;
../../../../WebCore/html/HTMLBodyElement.idl:                 attribute long scrollLeft;
../../../../WebCore/html/HTMLBodyElement.idl:                 attribute long scrollTop;
../../../../WebCore/html/HTMLButtonElement.idl:                 attribute  long                 tabIndex;
../../../../WebCore/html/HTMLCanvasElement.idl:        attribute long width;
../../../../WebCore/html/HTMLCanvasElement.idl:        attribute long height;
../../../../WebCore/html/HTMLEmbedElement.idl:                 attribute [ConvertFromString] long height;
../../../../WebCore/html/HTMLEmbedElement.idl:                 attribute [ConvertFromString] long width;
../../../../WebCore/html/HTMLImageElement.idl:                 attribute long height;
../../../../WebCore/html/HTMLImageElement.idl:                 attribute long hspace;
../../../../WebCore/html/HTMLImageElement.idl:                 attribute long vspace;
../../../../WebCore/html/HTMLImageElement.idl:                 attribute long width;
../../../../WebCore/html/HTMLInputElement.idl:                 attribute long            maxLength;
../../../../WebCore/html/HTMLInputElement.idl:                 attribute unsigned long   size; // Changed string -> long as part of DOM level 2
../../../../WebCore/html/HTMLInputElement.idl:                 attribute long            tabIndex;
../../../../WebCore/html/HTMLInputElement.idl:                 attribute long            selectionStart;
../../../../WebCore/html/HTMLInputElement.idl:                 attribute long            selectionEnd;
../../../../WebCore/html/HTMLLIElement.idl:        attribute long value;    
../../../../WebCore/html/HTMLMediaElement.idl:    attribute unsigned long playCount
../../../../WebCore/html/HTMLMediaElement.idl:    attribute unsigned long currentLoop;
../../../../WebCore/html/HTMLObjectElement.idl:                 attribute long            hspace;
../../../../WebCore/html/HTMLObjectElement.idl:                 attribute long            tabIndex;
../../../../WebCore/html/HTMLObjectElement.idl:                 attribute long            vspace;
../../../../WebCore/html/HTMLOListElement.idl:        attribute long start;
../../../../WebCore/html/HTMLOptionsCollection.idl:                 attribute long selectedIndex;
../../../../WebCore/html/HTMLOptionsCollection.idl:                 attribute [Custom] unsigned long length
../../../../WebCore/html/HTMLPreElement.idl:        attribute long width;
../../../../WebCore/html/HTMLSelectElement.idl:                 attribute long            selectedIndex;
../../../../WebCore/html/HTMLSelectElement.idl:                 attribute unsigned long   length
../../../../WebCore/html/HTMLSelectElement.idl:                 attribute long            size;
../../../../WebCore/html/HTMLSelectElement.idl:                 attribute long            tabIndex;
../../../../WebCore/html/HTMLTableCellElement.idl:                 attribute long            colSpan;
../../../../WebCore/html/HTMLTableCellElement.idl:                 attribute long            rowSpan;
../../../../WebCore/html/HTMLTableColElement.idl:        attribute long            span;
../../../../WebCore/html/HTMLTextAreaElement.idl:                 attribute  long                 cols;
../../../../WebCore/html/HTMLTextAreaElement.idl:                 attribute  long                 rows;
../../../../WebCore/html/HTMLTextAreaElement.idl:                 attribute  long                 tabIndex;
../../../../WebCore/html/HTMLTextAreaElement.idl:                 attribute long selectionStart;
../../../../WebCore/html/HTMLTextAreaElement.idl:                 attribute long selectionEnd;
../../../../WebCore/html/HTMLVideoElement.idl:        attribute long width;
../../../../WebCore/html/HTMLVideoElement.idl:        attribute long height;

../../../../WebCore/html/CanvasRenderingContext2D.idl:        attribute float globalAlpha;
../../../../WebCore/html/CanvasRenderingContext2D.idl:        attribute float lineWidth;
../../../../WebCore/html/CanvasRenderingContext2D.idl:        attribute float miterLimit;
../../../../WebCore/html/CanvasRenderingContext2D.idl:        attribute float shadowOffsetX;
../../../../WebCore/html/CanvasRenderingContext2D.idl:        attribute float shadowOffsetY;
../../../../WebCore/html/CanvasRenderingContext2D.idl:        attribute float shadowBlur;
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float currentTime
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float defaultPlaybackRate 
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float playbackRate 
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float start;
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float end;
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float loopStart;
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float loopEnd;
../../../../WebCore/html/HTMLMediaElement.idl:    attribute float volume 

../../../../WebCore/svg/SVGAnimatedInteger.idl:                 attribute long baseVal
../../../../WebCore/svg/SVGElementInstanceList.idl:        SVGElementInstance item(in unsigned long index);
../../../../WebCore/svg/SVGFilterElement.idl:        void setFilterRes(in unsigned long filterResX, in unsigned long filterResY);
../../../../WebCore/svg/SVGLengthList.idl:        SVGLength getItem(in unsigned long index)
../../../../WebCore/svg/SVGLengthList.idl:        SVGLength insertItemBefore(in SVGLength item, in unsigned long index)
../../../../WebCore/svg/SVGLengthList.idl:        SVGLength replaceItem(in SVGLength item, in unsigned long index)
../../../../WebCore/svg/SVGLengthList.idl:        SVGLength removeItem(in unsigned long index)
../../../../WebCore/svg/SVGNumberList.idl:        SVGNumber getItem(in unsigned long index)
../../../../WebCore/svg/SVGNumberList.idl:        SVGNumber insertItemBefore(in SVGNumber item, in unsigned long index)
../../../../WebCore/svg/SVGNumberList.idl:        SVGNumber replaceItem(in SVGNumber item, in unsigned long index)
../../../../WebCore/svg/SVGNumberList.idl:        SVGNumber removeItem(in unsigned long index)
../../../../WebCore/svg/SVGPathElement.idl:        unsigned long getPathSegAtLength(in float distance);
../../../../WebCore/svg/SVGPathSegList.idl:        [Custom] SVGPathSeg getItem(in unsigned long index)
../../../../WebCore/svg/SVGPathSegList.idl:        [Custom] SVGPathSeg insertItemBefore(in SVGPathSeg newItem, in unsigned long index)
../../../../WebCore/svg/SVGPathSegList.idl:        [Custom] SVGPathSeg replaceItem(in SVGPathSeg newItem, in unsigned long index)
../../../../WebCore/svg/SVGPathSegList.idl:        [Custom] SVGPathSeg removeItem(in unsigned long index)
../../../../WebCore/svg/SVGPointList.idl:        [Custom] SVGPoint getItem(in unsigned long index)
../../../../WebCore/svg/SVGPointList.idl:        [Custom] SVGPoint insertItemBefore(in SVGPoint item, in unsigned long index)
../../../../WebCore/svg/SVGPointList.idl:        [Custom] SVGPoint replaceItem(in SVGPoint item, in unsigned long index)
../../../../WebCore/svg/SVGPointList.idl:        [Custom] SVGPoint removeItem(in unsigned long index)
../../../../WebCore/svg/SVGStringList.idl:        core::DOMString getItem(in unsigned long index)
../../../../WebCore/svg/SVGStringList.idl:        core::DOMString insertItemBefore(in core::DOMString item, in unsigned long index)
../../../../WebCore/svg/SVGStringList.idl:        core::DOMString replaceItem(in core::DOMString item, in unsigned long index)
../../../../WebCore/svg/SVGStringList.idl:        core::DOMString removeItem(in unsigned long index)
../../../../WebCore/svg/SVGSVGElement.idl:        unsigned long suspendRedraw(in unsigned long maxWaitMilliseconds);
../../../../WebCore/svg/SVGSVGElement.idl:        void unsuspendRedraw(in unsigned long suspendHandleId)
../../../../WebCore/svg/SVGTextContentElement.idl:        long getNumberOfChars();
../../../../WebCore/svg/SVGTextContentElement.idl:        float getSubStringLength(in unsigned long offset, 
../../../../WebCore/svg/SVGTextContentElement.idl:                                 in unsigned long length)
../../../../WebCore/svg/SVGTextContentElement.idl:        SVGPoint getStartPositionOfChar(in unsigned long offset)
../../../../WebCore/svg/SVGTextContentElement.idl:        SVGPoint getEndPositionOfChar(in unsigned long offset)
../../../../WebCore/svg/SVGTextContentElement.idl:        SVGRect getExtentOfChar(in unsigned long offset)
../../../../WebCore/svg/SVGTextContentElement.idl:        float getRotationOfChar(in unsigned long offset)
../../../../WebCore/svg/SVGTextContentElement.idl:        long getCharNumAtPosition(in SVGPoint point);
../../../../WebCore/svg/SVGTextContentElement.idl:        void selectSubString(in unsigned long offset, 
../../../../WebCore/svg/SVGTextContentElement.idl:                             in unsigned long length)
../../../../WebCore/svg/SVGTransformList.idl:        [Custom] SVGTransform getItem(in unsigned long index)
../../../../WebCore/svg/SVGTransformList.idl:        [Custom] SVGTransform insertItemBefore(in SVGTransform item, in unsigned long index)
../../../../WebCore/svg/SVGTransformList.idl:        [Custom] SVGTransform replaceItem(in SVGTransform item, in unsigned long index)
../../../../WebCore/svg/SVGTransformList.idl:        [Custom] SVGTransform removeItem(in unsigned long index)
../../../../WebCore/xml/XPathResult.idl:        Node snapshotItem(in unsigned long index)

../../../../WebCore/svg/SVGAngle.idl:                                    in float valueInSpecifiedUnits);
../../../../WebCore/svg/SVGAnimationElement.idl:        float getStartTime();
../../../../WebCore/svg/SVGAnimationElement.idl:        float getCurrentTime();
../../../../WebCore/svg/SVGAnimationElement.idl:        float getSimpleDuration()
../../../../WebCore/svg/SVGFEGaussianBlurElement.idl:        void setStdDeviation(in float stdDeviationX, in float stdDeviationY);
../../../../WebCore/svg/SVGLength.idl:                                    in float valueInSpecifiedUnits);
../../../../WebCore/svg/SVGMatrix.idl:        [Custom] SVGMatrix translate(in float x, in float y);
../../../../WebCore/svg/SVGMatrix.idl:        [Custom] SVGMatrix scale(in float scaleFactor);
../../../../WebCore/svg/SVGMatrix.idl:        [Custom] SVGMatrix scaleNonUniform(in float scaleFactorX, in float scaleFactorY);
../../../../WebCore/svg/SVGMatrix.idl:        [Custom] SVGMatrix rotate(in float angle);
../../../../WebCore/svg/SVGMatrix.idl:        [Custom] SVGMatrix rotateFromVector(in float x, in float y)
../../../../WebCore/svg/SVGMatrix.idl:        [Custom] SVGMatrix skewX(in float angle);
../../../../WebCore/svg/SVGMatrix.idl:        [Custom] SVGMatrix skewY(in float angle);
../../../../WebCore/svg/SVGNumber.idl:    interface [Conditional=SVG, PODType=float] SVGNumber {
../../../../WebCore/svg/SVGPathElement.idl:        float getTotalLength();
../../../../WebCore/svg/SVGPathElement.idl:        SVGPoint getPointAtLength(in float distance);
../../../../WebCore/svg/SVGPathElement.idl:        unsigned long getPathSegAtLength(in float distance);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegMovetoAbs createSVGPathSegMovetoAbs(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                      in float y);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegMovetoRel createSVGPathSegMovetoRel(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                      in float y);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegLinetoAbs createSVGPathSegLinetoAbs(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                      in float y);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegLinetoRel createSVGPathSegLinetoRel(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                      in float y);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoCubicAbs createSVGPathSegCurvetoCubicAbs(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float x1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float y1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float x2, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float y2);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoCubicRel createSVGPathSegCurvetoCubicRel(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float x1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float y1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float x2, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                  in float y2);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoQuadraticAbs createSVGPathSegCurvetoQuadraticAbs(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                          in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                          in float x1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                          in float y1);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoQuadraticRel createSVGPathSegCurvetoQuadraticRel(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                          in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                          in float x1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                          in float y1);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegArcAbs createSVGPathSegArcAbs(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float r1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float r2, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float angle, 
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegArcRel createSVGPathSegArcRel(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float r1, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float r2, 
../../../../WebCore/svg/SVGPathElement.idl:                                                in float angle, 
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegLinetoHorizontalAbs createSVGPathSegLinetoHorizontalAbs(in float x);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegLinetoHorizontalRel createSVGPathSegLinetoHorizontalRel(in float x);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegLinetoVerticalAbs createSVGPathSegLinetoVerticalAbs(in float y);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegLinetoVerticalRel createSVGPathSegLinetoVerticalRel(in float y);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoCubicSmoothAbs createSVGPathSegCurvetoCubicSmoothAbs(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                              in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                              in float x2, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                              in float y2);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoCubicSmoothRel createSVGPathSegCurvetoCubicSmoothRel(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                              in float y, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                              in float x2, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                              in float y2);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoQuadraticSmoothAbs createSVGPathSegCurvetoQuadraticSmoothAbs(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                                      in float y);
../../../../WebCore/svg/SVGPathElement.idl:        SVGPathSegCurvetoQuadraticSmoothRel createSVGPathSegCurvetoQuadraticSmoothRel(in float x, 
../../../../WebCore/svg/SVGPathElement.idl:                                                                                      in float y);
../../../../WebCore/svg/SVGSVGElement.idl:        float getCurrentTime();
../../../../WebCore/svg/SVGSVGElement.idl:        void setCurrentTime(in float seconds);
../../../../WebCore/svg/SVGTextContentElement.idl:        float getComputedTextLength();
../../../../WebCore/svg/SVGTextContentElement.idl:        float getSubStringLength(in unsigned long offset, 
../../../../WebCore/svg/SVGTextContentElement.idl:        float getRotationOfChar(in unsigned long offset)
../../../../WebCore/svg/SVGTransform.idl:        void setTranslate(in float tx, in float ty);
../../../../WebCore/svg/SVGTransform.idl:        void setScale(in float sx, in float sy);
../../../../WebCore/svg/SVGTransform.idl:        void setRotate(in float angle, in float cx, in float cy);
../../../../WebCore/svg/SVGTransform.idl:        void setSkewX(in float angle);
../../../../WebCore/svg/SVGTransform.idl:        void setSkewY(in float angle);

../../../../WebCore/svg/SVGAngle.idl:                 attribute float          value;
../../../../WebCore/svg/SVGAngle.idl:                 attribute float          valueInSpecifiedUnits;
../../../../WebCore/svg/SVGAnimatedNumber.idl:                 attribute float baseVal
../../../../WebCore/svg/SVGLength.idl:                 attribute float          value;
../../../../WebCore/svg/SVGLength.idl:                 attribute float          valueInSpecifiedUnits;
../../../../WebCore/svg/SVGNumber.idl:                 attribute float value
../../../../WebCore/svg/SVGPathSegArcAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegArcAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegArcAbs.idl:                 attribute float   r1
../../../../WebCore/svg/SVGPathSegArcAbs.idl:                 attribute float   r2
../../../../WebCore/svg/SVGPathSegArcAbs.idl:                 attribute float   angle
../../../../WebCore/svg/SVGPathSegArcRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegArcRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegArcRel.idl:                 attribute float   r1
../../../../WebCore/svg/SVGPathSegArcRel.idl:                 attribute float   r2
../../../../WebCore/svg/SVGPathSegArcRel.idl:                 attribute float   angle
../../../../WebCore/svg/SVGPathSegCurvetoCubicAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoCubicAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegCurvetoCubicAbs.idl:                 attribute float   x1
../../../../WebCore/svg/SVGPathSegCurvetoCubicAbs.idl:                 attribute float   y1
../../../../WebCore/svg/SVGPathSegCurvetoCubicAbs.idl:                 attribute float   x2
../../../../WebCore/svg/SVGPathSegCurvetoCubicAbs.idl:                 attribute float   y2
../../../../WebCore/svg/SVGPathSegCurvetoCubicRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoCubicRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegCurvetoCubicRel.idl:                 attribute float   x1
../../../../WebCore/svg/SVGPathSegCurvetoCubicRel.idl:                 attribute float   y1
../../../../WebCore/svg/SVGPathSegCurvetoCubicRel.idl:                 attribute float   x2
../../../../WebCore/svg/SVGPathSegCurvetoCubicRel.idl:                 attribute float   y2
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothAbs.idl:                 attribute float   x2
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothAbs.idl:                 attribute float   y2
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothRel.idl:                 attribute float   x2
../../../../WebCore/svg/SVGPathSegCurvetoCubicSmoothRel.idl:                 attribute float   y2
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticAbs.idl:                 attribute float   x1
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticAbs.idl:                 attribute float   y1
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticRel.idl:                 attribute float   x1
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticRel.idl:                 attribute float   y1
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticSmoothRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegCurvetoQuadraticSmoothRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegLinetoAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegLinetoAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegLinetoHorizontalAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegLinetoHorizontalRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegLinetoRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegLinetoRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegLinetoVerticalAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegLinetoVerticalRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegMovetoAbs.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegMovetoAbs.idl:                 attribute float   y
../../../../WebCore/svg/SVGPathSegMovetoRel.idl:                 attribute float   x
../../../../WebCore/svg/SVGPathSegMovetoRel.idl:                 attribute float   y
../../../../WebCore/svg/SVGPoint.idl:                 attribute float x
../../../../WebCore/svg/SVGPoint.idl:                 attribute float y
../../../../WebCore/svg/SVGRect.idl:                 attribute float x
../../../../WebCore/svg/SVGRect.idl:                 attribute float y
../../../../WebCore/svg/SVGRect.idl:                 attribute float width
../../../../WebCore/svg/SVGRect.idl:                 attribute float height
../../../../WebCore/svg/SVGSVGElement.idl:                 attribute float currentScale

../../../../WebCore/svg/SVGMatrix.idl:        attribute double a;
../../../../WebCore/svg/SVGMatrix.idl:        attribute double b;
../../../../WebCore/svg/SVGMatrix.idl:        attribute double c;
../../../../WebCore/svg/SVGMatrix.idl:        attribute double d;
../../../../WebCore/svg/SVGMatrix.idl:        attribute double e;
../../../../WebCore/svg/SVGMatrix.idl:        attribute double f;

*/
