description('Tests to ensure MakeTextWritingDirection* modifies the direction of text and embed texts properly.');

if (!window.layoutTestController)
    testFailed('This test requires layoutTestController');

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function recursivelyRemoveExtraenousSpan(node) {
    for (var i = 0; i < node.childNodes.length; i++)
        recursivelyRemoveExtraenousSpan(node.childNodes[i]);
    if (node.localName == 'span' && node.attributes.length == 0) {
        var childNodes = node.childNodes;
        while (node.firstChild)
            node.parentNode.insertBefore(node.firstChild, node);
        node.parentNode.removeChild(node);
    }
}

function modifyWritingDirection(content, selector, command, expected)
{
    if (!window.layoutTestController)
        return;

    testContainer.innerHTML = content;
    var selected = selector(testContainer);
    window.layoutTestController.execCommand('MakeTextWritingDirection' + command);

    // Remove Apple-style-span because it does not need to be tested here.
    var actual = testContainer.innerHTML.replace(/ class="Apple-style-span"/g, '');
    var action = command + ' on ' + selected + ' of "' + content + '"';
    if (actual == expected)
        testPassed(action);
    else {
        error = ' yielded ' + actual + ' but expected ' + expected;
        recursivelyRemoveExtraenousSpan(testContainer);
        if (testContainer.innerHTML.replace(/ class="Apple-style-span"/g, '') == expected)
            testFailed('(due to the bug 44359) ' + action + error);
        else
            testFailed(action + error);
    }
}

function selectFirstWord(container) {
    document.getSelection().setBaseAndExtent(container, 0, container, 0);
    document.getSelection().modify('extend', 'forward', 'word');
    return 'first word';
}

function selectSecondAndThirdWords(container) {
    document.getSelection().setBaseAndExtent(container, 0, container, 0);
    document.getSelection().modify('move', 'forward', 'word');
    document.getSelection().modify('extend', 'forward', 'word');
    document.getSelection().modify('extend', 'forward', 'word');
    return 'second and third words';
}

function selectThirdWord(container) {
    document.getSelection().setBaseAndExtent(container, 0, container, 0);
    document.getSelection().modify('move', 'forward', 'word');
    document.getSelection().modify('move', 'forward', 'word');
    document.getSelection().modify('extend', 'forward', 'word');
    return 'third word';
}

// left to right language
modifyWritingDirection('hello world', selectFirstWord, 'Natural', 'hello world');
modifyWritingDirection('hello world', selectFirstWord, 'LeftToRight', '<span style="unicode-bidi: embed;">hello</span> world');
modifyWritingDirection('hello world', selectFirstWord, 'RightToLeft', '<span style="unicode-bidi: embed; direction: rtl;">hello</span> world');
modifyWritingDirection('<b>hello world</b> webkit', selectSecondAndThirdWords, 'Natural', '<b>hello world</b> webkit');
modifyWritingDirection('<b>hello world</b> webkit', selectSecondAndThirdWords, 'LeftToRight', '<b>hello<span style="unicode-bidi: embed;"> world</span></b><span style="unicode-bidi: embed;"> webkit</span>');
modifyWritingDirection('<b>hello world</b> webkit', selectSecondAndThirdWords, 'RightToLeft', '<b>hello<span style="unicode-bidi: embed; direction: rtl;"> world</span></b><span style="unicode-bidi: embed; direction: rtl;"> webkit</span>');
modifyWritingDirection('<span dir="rtl">hello <span dir="ltr">world webkit rocks</span></span>', selectThirdWord, 'Natural',
                       '<span dir="rtl">hello <span dir="ltr">world</span></span> webkit<span dir="rtl"><span dir="ltr"> rocks</span></span>');
modifyWritingDirection('<span dir="rtl">hello <span dir="ltr">world webkit rocks</span></span>', selectThirdWord, 'LeftToRight',
                       '<span dir="rtl">hello <span dir="ltr">world</span></span><span style="unicode-bidi: embed;"> webkit</span><span dir="rtl"><span dir="ltr"> rocks</span></span>');
modifyWritingDirection('<span dir="rtl">hello <span dir="ltr">world webkit rocks</span></span>', selectThirdWord, 'RightToLeft',
                       '<span dir="rtl">hello <span dir="ltr">world</span> webkit<span dir="ltr"> rocks</span></span>');

// right to left language
modifyWritingDirection('هنا يكتب النص العربي', selectFirstWord, 'Natural', 'هنا يكتب النص العربي');
modifyWritingDirection('هنا يكتب النص العربي', selectFirstWord, 'LeftToRight', '<span style="unicode-bidi: embed;">هنا</span> يكتب النص العربي');
modifyWritingDirection('هنا يكتب النص العربي', selectFirstWord, 'RightToLeft', '<span style="unicode-bidi: embed; direction: rtl;">هنا</span> يكتب النص العربي');

modifyWritingDirection('<b>هنا يكتب</b> النص العربي', selectSecondAndThirdWords, 'Natural', '<b>هنا يكتب</b> النص العربي');
modifyWritingDirection('<b>هنا يكتب</b> النص العربي', selectSecondAndThirdWords, 'LeftToRight', '<b>هنا<span style="unicode-bidi: embed;"> يكتب</span></b><span style="unicode-bidi: embed;"> النص</span> العربي');
modifyWritingDirection('<b>هنا يكتب</b> النص العربي', selectSecondAndThirdWords, 'RightToLeft', '<b>هنا<span style="unicode-bidi: embed; direction: rtl;"> يكتب</span></b><span style="unicode-bidi: embed; direction: rtl;"> النص</span> العربي');

modifyWritingDirection('<div dir="rtl">هنا يكتب النص العربي</div>', selectFirstWord, 'Natural', '<div dir="rtl">هنا يكتب النص العربي</div>');
modifyWritingDirection('<div dir="rtl">هنا يكتب النص العربي</div>', selectFirstWord, 'LeftToRight', '<div dir="rtl"><span style="unicode-bidi: embed; direction: ltr;">هنا</span> يكتب النص العربي</div>');
modifyWritingDirection('<div dir="rtl">هنا يكتب النص العربي</div>', selectFirstWord, 'RightToLeft', '<div dir="rtl"><span style="unicode-bidi: embed;">هنا</span> يكتب النص العربي</div>');

modifyWritingDirection('<div dir="rtl"><b>هنا يكتب</b> النص العربي</div>', selectSecondAndThirdWords, 'Natural', '<div dir="rtl"><b>هنا يكتب</b> النص العربي</div>');
modifyWritingDirection('<div dir="rtl"><b>هنا يكتب</b> النص العربي</div>', selectSecondAndThirdWords, 'LeftToRight', '<div dir="rtl"><b>هنا<span style="unicode-bidi: embed; direction: ltr;"> يكتب</span></b><span style="unicode-bidi: embed; direction: ltr;"> النص</span> العربي</div>');
modifyWritingDirection('<div dir="rtl"><b>هنا يكتب</b> النص العربي</div>', selectSecondAndThirdWords, 'RightToLeft', '<div dir="rtl"><b>هنا<span style="unicode-bidi: embed;"> يكتب</span></b><span style="unicode-bidi: embed;"> النص</span> العربي</div>');
modifyWritingDirection('<div dir="rtl">هنا <span dir="ltr">يكتب النص العربي</span></div>', selectThirdWord, 'Natural',
                       '<div dir="rtl">هنا <span dir="ltr">يكتب</span> النص<span dir="ltr"> العربي</span></div>');
modifyWritingDirection('<div dir="rtl">هنا <span dir="ltr">يكتب النص العربي</span></div>', selectThirdWord, 'LeftToRight',
                       '<div dir="rtl"><span style="direction: ltr;">هنا يكتب النص العربي</span></div>');
modifyWritingDirection('<div dir="rtl">هنا <span dir="ltr">يكتب النص العربي</span></div>', selectThirdWord, 'RightToLeft',
                       '<div dir="rtl">هنا <span dir="ltr">يكتب</span><span style="unicode-bidi: embed;"> النص</span><span dir="ltr"> العربي</span></div>');

// bidirectional langauge
modifyWritingDirection('写中文', selectFirstWord, 'Natural', '写中文');
modifyWritingDirection('写中文', selectFirstWord, 'LeftToRight', '<span style="unicode-bidi: embed;">写</span>中文');
modifyWritingDirection('写中文', selectFirstWord, 'RightToLeft', '<span style="unicode-bidi: embed; direction: rtl;">写</span>中文');

modifyWritingDirection('<div dir="rtl">写中文</div>', selectFirstWord, 'Natural', '<div dir="rtl">写中文</div>');
modifyWritingDirection('<div dir="rtl">写中文</div>', selectFirstWord, 'LeftToRight', '<div dir="rtl"><span style="unicode-bidi: embed; direction: ltr;">写</span>中文</div>');
modifyWritingDirection('<div dir="rtl">写中文</div>', selectFirstWord, 'RightToLeft', '<div dir="rtl"><span style="unicode-bidi: embed;">写</span>中文</div>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
