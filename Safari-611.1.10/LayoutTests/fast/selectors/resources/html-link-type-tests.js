function testHTMLElement(tagName, selector, shouldMatch) {
    debug('Testing ' + tagName)
    var element = document.createElement(tagName);
    element.id = "target";
    element.className = "target";
    document.documentElement.appendChild(element);

    // An element without href never matches.
    shouldBeFalse('document.getElementById("target").matches("' + selector + '")');
    shouldBe('document.querySelectorAll("#target' + selector + '").length', '0');
    shouldBe('document.querySelectorAll(".target' + selector + '").length', '0');

    var testFunction = shouldMatch ? shouldBeTrue : shouldBeFalse;
    // Any value of href attribute should match.
    element.setAttribute('href', '');
    testFunction('document.getElementById("target").matches("' + selector + '")');
    shouldBe('document.querySelectorAll("#target' + selector + '").length', shouldMatch ? '1' : '0');
    shouldBe('document.querySelectorAll(".target' + selector + '").length', shouldMatch ? '1' : '0');

    element.setAttribute('href', 'http://www.webkit.org');
    testFunction('document.getElementById("target").matches("' + selector + '")');
    shouldBe('document.querySelectorAll("#target' + selector + '").length', shouldMatch ? '1' : '0');
    shouldBe('document.querySelectorAll(".target' + selector + '").length', shouldMatch ? '1' : '0');

    element.removeAttribute('href');
    shouldBeFalse('document.getElementById("target").matches("' + selector + '")');
    shouldBe('document.querySelectorAll("#target' + selector + '").length', '0');
    shouldBe('document.querySelectorAll(".target' + selector + '").length', '0');

    element.setAttributeNS('http://www.webkit.org', 'href', 'http://www.webkit.org');
    shouldBeFalse('document.getElementById("target").matches("' + selector + '")');
    shouldBe('document.querySelectorAll("#target' + selector + '").length', '0');
    shouldBe('document.querySelectorAll(".target' + selector + '").length', '0');

    document.documentElement.removeChild(element);
}

htmlTags = ['a', 'abbr', 'acronym', 'address', 'applet', 'area', 'article', 'aside', 'audio', 'b', 'base', 'basefont', 'bdi', 'bdo', 'bgsound', 'big', 'blockquote', 'body', 'br', 'button', 'canvas', 'caption', 'center', 'cite', 'code', 'col', 'colgroup', 'command', 'webkitShadowContent', 'datalist', 'dd', 'del', 'details', 'dfn', 'dir', 'div', 'dl', 'dt', 'em', 'embed', 'fieldset', 'figcaption', 'figure', 'font', 'footer', 'form', 'frame', 'frameset', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'head', 'header', 'hgroup', 'hr', 'html', 'i', 'iframe', 'image', 'img', 'input', 'ins', 'isindex', 'kbd', 'keygen', 'label', 'layer', 'legend', 'li', 'link', 'listing', 'main', 'map', 'mark', 'marquee', 'menu', 'meta', 'meter', 'nav', 'nobr', 'noembed', 'noframes', 'nolayer', 'object', 'ol', 'optgroup', 'option', 'output', 'p', 'param', 'plaintext', 'pre', 'progress', 'q', 'rb', 'rp', 'rt', 'rtc', 'ruby', 's', 'samp', 'script', 'section', 'select', 'small', 'source', 'span', 'strike', 'strong', 'style', 'sub', 'summary', 'sup', 'table', 'tbody', 'td', 'template', 'textarea', 'tfoot', 'th', 'thead', 'title', 'tr', 'track', 'tt', 'u', 'ul', 'var', 'video', 'wbr', 'xmp', 'noscript'];

function testHTMLTagsForLink(selector) {
    for (var i = 0; i < htmlTags.length; ++i) {
        var tag = htmlTags[i];
        var shouldMatch = tag === 'a' || tag === 'area' || tag === 'link';
        testHTMLElement(tag, selector, shouldMatch);
    }
}