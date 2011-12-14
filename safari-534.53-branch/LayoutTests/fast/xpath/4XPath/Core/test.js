DOM = (new DOMParser).parseFromString(
    '<?xml version="1.0" encoding="ISO-8859-1"?>' +
    '<!DOCTYPE ROOT [' +
    '  <!ELEMENT CHILD2 (#PCDATA|GCHILD)*>' +
    '  <!ATTLIST CHILD2 attr1 CDATA #IMPLIED' +
    '                   CODE ID #REQUIRED>' +
    ']>' +
    '<?xml-stylesheet "Data" ?>' +
    '<ROOT>' +
    '  <!-- Test Comment -->' +
    '  <CHILD1 attr1="val1" attr31="31">' +
    '    <GCHILD name="GCHILD11"/>' +
    '    <GCHILD name="GCHILD12"/>' +
    '    Text1' +
    '  </CHILD1>' +
    '  <CHILD2 attr1="val2" CODE="1">' +
    '    <GCHILD name="GCHILD21"/>' +
    '    <GCHILD name="GCHILD22"/>' +
    '  </CHILD2>' +
    '  <foo:CHILD3 xmlns:foo="http://foo.com" foo:name="mike"/>' +
    '  <lang xml:lang="en">' +
    '    <foo xml:lang=""/>' +
    '    <foo/>' +
    '    <f\xf6\xf8/>' +
    '  </lang>' +
    '</ROOT>' +
    '<?no-data ?>',
    'application/xml');

DOM = DOM;
ROOT = DOM.documentElement;

PI = DOM.firstChild;
while (PI.nodeType != Node.PROCESSING_INSTRUCTION_NODE)
    PI = PI.nextSibling;

PI2 = DOM.lastChild;
COMMENT = ROOT.firstChild
while (COMMENT.nodeType != Node.COMMENT_NODE)
    COMMENT = COMMENT.nextSibling;

CHILD1 = DOM.getElementsByTagName("CHILD1")[0];
ATTR1 = CHILD1.getAttributeNode("attr1");
ATTR31 = CHILD1.getAttributeNode("attr31");
CHILD2 = DOM.getElementsByTagName("CHILD2")[0];
ATTR2 = CHILD2.getAttributeNode("attr1");
IDATTR2 = CHILD2.getAttributeNode('CODE')
CHILD3 = DOM.getElementsByTagName("CHILD3")[0];
if (!CHILD3)
    CHILD3 = DOM.getElementsByTagName("foo:CHILD3")[0];
text = CHILD1.lastChild;
LANG = DOM.getElementsByTagName("lang")[0];
NONASCIIQNAME = DOM.getElementsByTagName("f\xf6\xf8")[0];

CHILDREN = [CHILD1, CHILD2, CHILD3, LANG];
GCHILDREN1 = [CHILD1.getElementsByTagName("GCHILD")[0], CHILD1.getElementsByTagName("GCHILD")[1]];
GCHILD11 = GCHILDREN1[0];
GCHILD12 = GCHILDREN1[1];
TEXT1 = CHILD1.lastChild;
GCHILDREN2 = [CHILD2.getElementsByTagName("GCHILD")[0], CHILD2.getElementsByTagName("GCHILD")[1]];
GCHILD21 = GCHILDREN2[0];
GCHILD22 = GCHILDREN2[1];
LCHILDREN = [LANG.getElementsByTagName("foo")[0], LANG.getElementsByTagName("foo")[1], LANG.getElementsByTagName("f\xf6\xf8")[0]];
LCHILD1 = LCHILDREN[0];
LCHILD2 = LCHILDREN[1];
