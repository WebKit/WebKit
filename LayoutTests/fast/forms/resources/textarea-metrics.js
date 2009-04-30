description("This test checks that textareas have the right metrics. These numbers match IE7 except for scrollHeight. For two reasons:<br>" +
"1. scrollHeight is different for elements without enough content to cause scroll because IE7 then reports the height of the text inside the " +
"element as the scrollHeight. IE8 reports has scrollHeight == offsetHeight. Gecko/WebKit have scrollHeight == clientHeight.<br>" +
"2. For the elements with scroll in standards-mode, IE wraps the text differently. It seems to leave 2px less space for the text. We don't " +
"currently mimic this quirk. It's not clear whether we should given that we agree with IE7's clientWidth numbers in all these cases.");

function assertTextareaMetrics(doc, properties, expectedMetrics) {
    var textarea = doc.createElement('textarea');
    // Give some consistent CSS for consistent rendering across platforms
    // and to help in reasoning when trying to match IE metrics.
    var style = 'overflow-y:auto; font-family:Ahem; line-height:20px; height:50px; width:50px;';
    var title = '';
    for (property in properties) {
        var value = properties[property];
        title += ' ' + property + ': "' + value + '",';
        if (property == 'style')
            style += value;
        else
            textarea[property] = value;
    }
    textarea.style.cssText = style;
    doc.body.appendChild(textarea);

    // Create a more human-readable ID.
    var id = title.replace(/[\'\",;\:]/g, ' ').replace(/ +/g, '-');
    id = id == '' ? 'no-styles' : id;
    textarea.id = id;

    window[doc.compatMode + 'doc'] = doc;

    function assertMetricsForTextarea() {
        if (!title)
            title = ' none';

        debug('Properties =' + title);
        shouldBe(doc.compatMode + "doc.getElementById('" + id + "').clientWidth", String(expectedMetrics.clientWidth));
        shouldBe(doc.compatMode + "doc.getElementById('" + id + "').clientHeight", String(expectedMetrics.clientHeight));
        shouldBe(doc.compatMode + "doc.getElementById('" + id + "').offsetWidth", String(expectedMetrics.offsetWidth));
        shouldBe(doc.compatMode + "doc.getElementById('" + id + "').offsetHeight", String(expectedMetrics.offsetHeight));
        shouldBe(doc.compatMode + "doc.getElementById('" + id + "').scrollWidth", String(expectedMetrics.scrollWidth));
        shouldBe(doc.compatMode + "doc.getElementById('" + id + "').scrollHeight", String(expectedMetrics.scrollHeight));
        debug('');
    }
    if (document.all)
      // Give a timeout so IE has time to figure out it's metrics.
      setTimeout(assertMetricsForTextarea, 100);
    else
      assertMetricsForTextarea();
}

var smallHTML = 'A';
var htmlThatCausesScroll = 'AAAAAAAAA';

function testTextareasForDocument(doc, compatMode,
        textareaSizes, textareaWithScrollSizes,
        textareaWith8pxPaddingSizes, textareaWith8pxPaddingAndScrollbarSizes) {
    if (doc.compatMode != compatMode)
        testFailed('This doc should be in ' + compatMode + ' mode.');

    try {
        var scrollbarStyle = doc.createElement('style');
        scrollbarStyle.innerText = 'textarea::-webkit-scrollbar{ width:17px }';
        doc.getElementsByTagName('head')[0].appendChild(scrollbarStyle);
    } catch (e) {
        // IE throws an exception here, but doesn't need the above clause anyways.
    }

    debug('Testing ' + compatMode + ' document.')
    assertTextareaMetrics(doc, {}, textareaSizes);
    assertTextareaMetrics(doc, {disabled: true}, textareaSizes);
    assertTextareaMetrics(doc, {innerHTML: smallHTML}, textareaSizes);
    assertTextareaMetrics(doc, {innerHTML: htmlThatCausesScroll}, textareaWithScrollSizes);
    assertTextareaMetrics(doc, {innerHTML: smallHTML, disabled: true}, textareaSizes);
    assertTextareaMetrics(doc, {innerHTML: htmlThatCausesScroll, disabled: true}, textareaWithScrollSizes);
    assertTextareaMetrics(doc, {innerHTML: smallHTML, style: 'padding:8px'}, textareaWith8pxPaddingSizes);
    assertTextareaMetrics(doc, {innerHTML: htmlThatCausesScroll, style: 'padding:8px'}, textareaWith8pxPaddingAndScrollbarSizes);
    assertTextareaMetrics(doc, {innerHTML: smallHTML, rows: 10}, textareaSizes);
    assertTextareaMetrics(doc, {innerHTML: htmlThatCausesScroll, rows: 10}, textareaWithScrollSizes);
}

// For textareas with scrollbars have the expected clientWidth be the
// expected offsetWidth - scrollbarPlusBorderWidth.
// default border on textareas is 1px solid. So, the border width is 2.
// And the scrollbarWidth we set to be 17 to match windows so that
// these numbers will be platform independent and match IE.
var scrollbarPlusBorderWidth = 19;

var textareaSizesQuirks = {clientWidth: 48,
                           clientHeight: 48,
                           offsetWidth: 50,
                           offsetHeight: 50,
                           scrollWidth: 48,
                           scrollHeight: 48};

var textareaWithScrollSizesQuirks = {clientWidth: 50 - scrollbarPlusBorderWidth,
                                     clientHeight: 48,
                                     offsetWidth: 50,
                                     offsetHeight: 50,
                                     scrollWidth: 50 - scrollbarPlusBorderWidth,
                                     scrollHeight: 104};

var textareaWith8pxPaddingSizesQuirks = {clientWidth: 48,
                                         clientHeight: 48,
                                         offsetWidth: 50,
                                         offsetHeight: 50,
                                         scrollWidth: 48,
                                         scrollHeight: 48};

var textareaWith8pxPaddingAndScrollbarSizesQuirks = {clientWidth: 50 - scrollbarPlusBorderWidth,
                                                     clientHeight: 48,
                                                     offsetWidth: 50,
                                                     offsetHeight: 50,
                                                     scrollWidth: 50 - scrollbarPlusBorderWidth,
                                                     scrollHeight: 196};

testTextareasForDocument(document, 'BackCompat',
                         textareaSizesQuirks, textareaWithScrollSizesQuirks,
                         textareaWith8pxPaddingSizesQuirks, textareaWith8pxPaddingAndScrollbarSizesQuirks);

var standardsIframe = document.createElement('iframe');
standardsIframe.style.width = '100%';
document.body.appendChild(standardsIframe);
standardsIframe.contentWindow.document.write('<!DocType html><html><head></head><body></body></html>');
standardsIframe.contentWindow.document.close();

var textareaSizesStandards = {clientWidth: 54,
                              clientHeight: 54,
                              offsetWidth: 56,
                              offsetHeight: 56,
                              scrollWidth: 54,
                              scrollHeight: 54};

var textareaWithScrollSizesStandards = {clientWidth: 56 - scrollbarPlusBorderWidth,
                                        clientHeight: 54,
                                        offsetWidth: 56,
                                        offsetHeight: 56,
                                        scrollWidth: 56 - scrollbarPlusBorderWidth,
                                        scrollHeight: 64};

var textareaWith8pxPaddingSizesStandards = {clientWidth: 66,
                                            clientHeight: 66,
                                            offsetWidth: 68,
                                            offsetHeight: 68,
                                            scrollWidth: 66,
                                            scrollHeight: 66};

var textareaWith8pxPaddingAndScrollbarSizesStandards = {clientWidth: 68 - scrollbarPlusBorderWidth,
                                                        clientHeight: 66,
                                                        offsetWidth: 68,
                                                        offsetHeight: 68,
                                                        scrollWidth: 68 - scrollbarPlusBorderWidth,
                                                        scrollHeight: 76};

testTextareasForDocument(standardsIframe.contentWindow.document, 'CSS1Compat',
                         textareaSizesStandards, textareaWithScrollSizesStandards,
                         textareaWith8pxPaddingSizesStandards, textareaWith8pxPaddingAndScrollbarSizesStandards);

var successfullyParsed = true;
