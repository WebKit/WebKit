description("This test measures the width of textareas and text inputs for different fonts.");

var sizes = [1, 2, 3, 4, 5, 10, 20, 50, 100, 500, 1000];
// This list was grabbed from Wikipedia
// http://en.wikipedia.org/wiki/Core_fonts_for_the_Web
// Impact was removed from the list as not all versions seem to have the same metrics
var ms_web_fonts = ['Andale Mono', 'Arial', 'Comic Sans MS', 'Courier New', 'Georgia',
                    'Times New Roman', 'Trebuchet MS', 'Verdana', 'Webdings'];

// These are fonts we expect to see installed on all systems.
var fonts = ['Lucida Grande', 'Courier', 'Helvetica', 'Monaco', 'Times'].concat(ms_web_fonts);

function printElementWidth(tagname, font) {
    debug('<b>' + tagname + '</b>');
    var node = document.createElement(tagname);
    node.style.fontFamily = font;
    document.body.appendChild(node);
    var sizeProperty = tagname == 'input' ? 'size' : 'cols';
    for (var i = 0; i < sizes.length; i++) {
        node[sizeProperty] = sizes[i];
        debug(sizeProperty + '=' + sizes[i] + ' clientWidth=' + node.clientWidth);
    }
    document.body.removeChild(node);
}

for (var j = 0; j < fonts.length; j++) {
    debug('<b>' + fonts[j] + '</b>');
    printElementWidth('input', fonts[j]);
    debug('')
    printElementWidth('textarea', fonts[j]);
    debug('');
}

var successfullyParsed = true;
