description("Tests queryCommandValue('formatBlock')")

var testContainer = document.createElement("span");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function queryFormatBlock(selector, content, expected)
{
    testContainer.innerHTML = content;
    var selected = selector(testContainer);
    var actual = document.queryCommandValue('formatBlock');
    var action = "queryCommand('formatBlock') returned \"" + actual + '" selecting ' + selected + ' of "' + content + '"';
    if (actual == expected)
        testPassed(action);
    else
        testFailed(action + ' but expected "' + expected + '"');
}

function selectFirstPosition(container) {
    while (container.firstChild)
        container = container.firstChild;
    window.getSelection().setPosition(container, 0);
    return 'first position'
}

function selectMiddleOfHelloWorld(container) {
    window.getSelection().setPosition(container, 0);
    window.getSelection().modify('move', 'forward', 'character');
    window.getSelection().modify('move', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'character');
    window.getSelection().modify('extend', 'forward', 'character');
    return '"llo wo"'
}

debug('Basic cases');
queryFormatBlock(function () {return 'none' }, 'hello', '');
queryFormatBlock(selectFirstPosition, 'hello', '');
queryFormatBlock(selectFirstPosition, '<div contenteditable="false"><h1 contenteditable>hello</h1></div>', '');
queryFormatBlock(selectFirstPosition, '<h1 contenteditable="false"><span contenteditable>hello</span></h1>', '');
queryFormatBlock(selectFirstPosition, '<div><h1 contenteditable="false"><span contenteditable>hello</span></h1></div>', '');
queryFormatBlock(selectFirstPosition, '<div><header contenteditable="false"><h1 contenteditable>hello</h1></header></div>', '');
queryFormatBlock(selectFirstPosition, '<a>hello</a>', '');
queryFormatBlock(selectFirstPosition, '<abbr>hello</abbr>', '');
queryFormatBlock(selectFirstPosition, '<acronym>hello</acronym>', '');
queryFormatBlock(selectFirstPosition, '<b>hello</b>', '');
queryFormatBlock(selectFirstPosition, '<bdo>hello</bdo>', '');
queryFormatBlock(selectFirstPosition, '<big>hello</big>', '');
queryFormatBlock(selectFirstPosition, '<button>hello</button>', '');
queryFormatBlock(selectFirstPosition, '<caption>hello</caption>', '');
queryFormatBlock(selectFirstPosition, '<center>hello</center>', '');
queryFormatBlock(selectFirstPosition, '<cite>hello</cite>', '');
queryFormatBlock(selectFirstPosition, '<code>hello</code>', '');
queryFormatBlock(selectFirstPosition, '<del>hello</del>', '');
queryFormatBlock(selectFirstPosition, '<dfn>hello</dfn>', '');
queryFormatBlock(selectFirstPosition, '<dir>hello</dir>', '');
queryFormatBlock(selectFirstPosition, '<em>hello</em>', '');
queryFormatBlock(selectFirstPosition, '<form><fieldset>hello</fieldset></form>', '');
queryFormatBlock(selectFirstPosition, '<font>hello</font>', '');
queryFormatBlock(selectFirstPosition, '<form>hello</form>', '');
queryFormatBlock(selectFirstPosition, '<i>hello</i>', '');
queryFormatBlock(selectFirstPosition, '<kbd>hello</kbd>', '');
queryFormatBlock(selectFirstPosition, '<label>hello</label>', '');
queryFormatBlock(selectFirstPosition, '<legend>hello</legend>', '');
queryFormatBlock(selectFirstPosition, '<ul><li>hello<li></ul>', '');
queryFormatBlock(selectFirstPosition, '<menu>hello</menu>', '');
queryFormatBlock(selectFirstPosition, '<ol>hello</ol>', '');
queryFormatBlock(selectFirstPosition, '<q>hello</q>', '');
queryFormatBlock(selectFirstPosition, '<s>hello</s>', '');
queryFormatBlock(selectFirstPosition, '<samp>hello</samp>', '');
queryFormatBlock(selectFirstPosition, '<small>hello</small>', '');
queryFormatBlock(selectFirstPosition, '<span>hello</span>', '');
queryFormatBlock(selectFirstPosition, '<strike>hello</strike>', '');
queryFormatBlock(selectFirstPosition, '<strong>hello</strong>', '');
queryFormatBlock(selectFirstPosition, '<sub>hello</sub>', '');
queryFormatBlock(selectFirstPosition, '<sup>hello</sup>', '');
queryFormatBlock(selectFirstPosition, '<table><td>hello</td></table>', '');
queryFormatBlock(selectFirstPosition, '<table><th>hello</th></table>', '');
queryFormatBlock(selectFirstPosition, '<tt>hello</tt>', '');
queryFormatBlock(selectFirstPosition, '<u>hello</u>', '');
queryFormatBlock(selectFirstPosition, '<ul>hello</ul>', '');
queryFormatBlock(selectFirstPosition, '<var>hello</var>', '');

queryFormatBlock(selectFirstPosition, '<address>hello</address>', 'address');
queryFormatBlock(selectFirstPosition, '<article>hello</article>', 'article');
queryFormatBlock(selectFirstPosition, '<aside>hello</aside>', 'aside');
queryFormatBlock(selectFirstPosition, '<blockquote>hello</blockquote>', 'blockquote');
queryFormatBlock(selectFirstPosition, '<dd>hello</dd>', 'dd');
queryFormatBlock(selectFirstPosition, '<dl><dd>hello</dd></dl>', 'dd');
queryFormatBlock(selectFirstPosition, '<div>hello</div>', 'div');
queryFormatBlock(selectFirstPosition, '<dl>hello</dl>', 'dl');
queryFormatBlock(selectFirstPosition, '<dt>hello</dt>', 'dt');
queryFormatBlock(selectFirstPosition, '<dl><dt>hello</dt></dl>', 'dt');
queryFormatBlock(selectFirstPosition, '<footer>hello</footer>', 'footer');
queryFormatBlock(selectFirstPosition, '<h1>hello</h1>', 'h1');
queryFormatBlock(selectFirstPosition, '<h2>hello</h2>', 'h2');
queryFormatBlock(selectFirstPosition, '<h3>hello</h3>', 'h3');
queryFormatBlock(selectFirstPosition, '<h4>hello</h4>', 'h4');
queryFormatBlock(selectFirstPosition, '<h5>hello</h5>', 'h5');
queryFormatBlock(selectFirstPosition, '<h6>hello</h6>', 'h6');
queryFormatBlock(selectFirstPosition, '<header>hello</header>', 'header');
queryFormatBlock(selectFirstPosition, '<hgroup>hello</hgroup>', 'hgroup');
queryFormatBlock(selectFirstPosition, '<nav>hello</nav>', 'nav');
queryFormatBlock(selectFirstPosition, '<p>hello</p>', 'p');
queryFormatBlock(selectFirstPosition, '<pre>hello</pre>', 'pre');
queryFormatBlock(selectFirstPosition, '<section>hello</section>', 'section');

debug('');
debug('Nested cases');
queryFormatBlock(selectFirstPosition, '<h1><h2>hello</h2></h1>', 'h2');
queryFormatBlock(selectFirstPosition, '<h1><address>hello</address></h1>', 'address');
queryFormatBlock(selectFirstPosition, '<pre>hello<p>world</p></pre>', 'pre');
queryFormatBlock(selectMiddleOfHelloWorld, '<pre>hello<p>world</p></pre>', 'pre');
queryFormatBlock(selectMiddleOfHelloWorld, '<address><h1>hello</h1>world</address>', 'address');
queryFormatBlock(selectMiddleOfHelloWorld, '<h1>hello</h1>world', '');

document.body.removeChild(testContainer);
var successfullyParsed = true;
