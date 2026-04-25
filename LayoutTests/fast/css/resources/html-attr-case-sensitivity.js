description("This tests that [attr=value] CSS selectors are case sensitive depending on attr");

// List of all HTML4 attrs
// true = case sensitive
var htmlAttrs = {
    "abbr" : true,
    "accept-charset" : false,
    "accept" : false,
    "accesskey" : true,
    "action" : true,
    "align" : false,
    "alink" : false,
    "alt" : true,
    "archive" : true,
    "axis" : false,
    "background" : true,
    "bgcolor" : false,
    "border" : true,
    "cellpadding" : true,
    "cellspacing" : true,
    "char" : true,
    "charoff" : true,
    "charset" : false,
    "checked" : false,
    "cite" : true,
    "class" : true,
    "classid" : true,
    "clear" : false,
    "code" : true,
    "codebase" : true,
    "codetype" : false,
    "color" : false,
    "cols" : true,
    "colspan" : true,
    "compact" : false,
    "content" : true,
    "coords" : true,
    "data" : true,
    "datetime" : true,
    "declare" : false,
    "defer" : false,
    "dir" : false,
    "disabled" : false,
    "enctype" : false,
    "face" : false,
    "for" : true,
    "frame" : false,
    "frameborder" : true,
    "headers" : true,
    "height" : true,
    "href" : true,
    "hreflang" : false,
    "hspace" : true,
    "http-equiv" : false,
    "id" : true,
    "ismap" : true,
    "label" : true,
    "lang" : false,
    "language" : false,
    "link" : false,
    "longdesc" : true,
    "marginheight" : true,
    "marginwidth" : true,
    "maxlength" : true,
    "media" : false,
    "method" : false,
    "multiple" : false,
    "name" : true,
    "nohref" : false,
    "noresize" : false,
    "noshade" : false,
    "nowrap" : false,
    "object" : true,
    "onblur" : true,
    "onchange" : true,
    "onclick" : true,
    "ondblclick" : true,
    "onfocus" : true,
    "onkeydown" : true,
    "onkeypress" : true,
    "onkeyup" : true,
    "onload" : true,
    "onmousedown" : true,
    "onmousemove" : true,
    "onmouseout" : true,
    "onmouseover" : true,
    "onmouseup" : true,
    "onreset" : true,
    "onselect" : true,
    "onsubmit" : true,
    "onunload" : true,
    "profile" : true,
    "prompt" : true,
    "readonly" : false,
    "rel" : false,
    "rev" : false,
    "rows" : true,
    "rowspan" : true,
    "rules" : false,
    "scheme" : true,
    "scope" : false,
    "scrolling" : false,
    "selected" : false,
    "shape" : false,
    "size" : true,
    "span" : true,
    "src" : true,
    "standby" : true,
    "start" : true,
    "style" : true,
    "summary" : true,
    "tabindex" : true,
    "target" : false,
    "text" : false,
    "title" : true,
    "type" : false,
    "usemap" : true,
    "valign" : false,
    "value" : true,
    "valuetype" : false,
    "version" : true,
    "vlink" : false,
    "vspace" : true,
    "width" : true
};

function isCaseSensitive(attrName) {
    var style = document.createElement('style');
    style.appendChild(document.createTextNode("br { color: red; float: right; border-color: red; }\n"));
    style.appendChild(document.createTextNode("br[" + attrName + "=foo] { color: blue; }\n"));
    style.appendChild(document.createTextNode("br[" + attrName + "~=foo] { float: left; }\n"));
    style.appendChild(document.createTextNode("br[" + attrName + "|=foo] { border-left-color: green; }\n"));
    style.appendChild(document.createTextNode("br[" + attrName + "^=foo] { border-right-color: pink; }\n"));
    style.appendChild(document.createTextNode("br[" + attrName + "$=foo] { border-top-color: orange; }\n"));
    style.appendChild(document.createTextNode("br[" + attrName + "*=foo] { border-bottom-color: black; }\n"));
    
    document.documentElement.firstChild.appendChild(style);
    
    var testElement = document.createElement("br");
    testElement.setAttribute(attrName, "FOO");
    document.body.appendChild(testElement);
    
    var computedStyle = window.getComputedStyle(testElement, '');
    
    var isCaseInsensitive = (computedStyle.getPropertyValue("color") == "rgb(0, 0, 255)");
    
    if ((computedStyle.getPropertyValue("float") == "left") != isCaseInsensitive)
        testFailed("~=foo and =foo for " + attrName + " did not match with same case sensitivity");
    
    if ((computedStyle.getPropertyValue("border-left-color") == "rgb(0, 128, 0)") != isCaseInsensitive)
        testFailed("|=foo and =foo for " + attrName + " did not match with same case sensitivity");

    if ((computedStyle.getPropertyValue("border-right-color") == "rgb(255, 192, 203)") != isCaseInsensitive)
        testFailed("^=foo and =foo for " + attrName + " did not match with same case sensitivity");

    if ((computedStyle.getPropertyValue("border-top-color") == "rgb(255, 165, 0)") != isCaseInsensitive)
        testFailed("$=foo and =foo for " + attrName + " did not match with same case sensitivity");

    if ((computedStyle.getPropertyValue("border-bottom-color") == "rgb(0, 0, 0)") != isCaseInsensitive)
        testFailed("*=foo and =foo for " + attrName + " did not match with same case sensitivity");
    
    // clean up
    testElement.parentNode.removeChild(testElement);
    style.parentNode.removeChild(style);
    
    return !isCaseInsensitive;
}

for (var attr in htmlAttrs) {
    shouldBe('isCaseSensitive("' + attr + '")', "" + htmlAttrs[attr]);
}

// test a nonexistent attr
shouldBe('isCaseSensitive("foobar")', "true");
