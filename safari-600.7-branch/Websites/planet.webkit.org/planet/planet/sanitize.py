"""
sanitize: bringing sanitiy to world of messed-up data
"""

__author__ = ["Mark Pilgrim <http://diveintomark.org/>", 
              "Aaron Swartz <http://www.aaronsw.com/>"]
__contributors__ = ["Sam Ruby <http://intertwingly.net/>"]
__license__ = "BSD"
__version__ = "0.25"

_debug = 0

# If you want sanitize to automatically run HTML markup through HTML Tidy, set
# this to 1.  Requires mxTidy <http://www.egenix.com/files/python/mxTidy.html>
# or utidylib <http://utidylib.berlios.de/>.
TIDY_MARKUP = 0

# List of Python interfaces for HTML Tidy, in order of preference.  Only useful
# if TIDY_MARKUP = 1
PREFERRED_TIDY_INTERFACES = ["uTidy", "mxTidy"]

import sgmllib, re

# chardet library auto-detects character encodings
# Download from http://chardet.feedparser.org/
try:
    import chardet
    if _debug:
        import chardet.constants
        chardet.constants._debug = 1

    _chardet = lambda data: chardet.detect(data)['encoding']
except:
    chardet = None
    _chardet = lambda data: None

class _BaseHTMLProcessor(sgmllib.SGMLParser):
    elements_no_end_tag = ['area', 'base', 'basefont', 'br', 'col', 'frame', 'hr',
      'img', 'input', 'isindex', 'link', 'meta', 'param']
    
    _r_barebang = re.compile(r'<!((?!DOCTYPE|--|\[))', re.IGNORECASE)
    _r_bareamp = re.compile("&(?!#\d+;|#x[0-9a-fA-F]+;|\w+;)")
    _r_shorttag = re.compile(r'<([^<\s]+?)\s*/>')
    
    def __init__(self, encoding):
        self.encoding = encoding
        if _debug: sys.stderr.write('entering BaseHTMLProcessor, encoding=%s\n' % self.encoding)
        sgmllib.SGMLParser.__init__(self)
        
    def reset(self):
        self.pieces = []
        sgmllib.SGMLParser.reset(self)

    def _shorttag_replace(self, match):
        tag = match.group(1)
        if tag in self.elements_no_end_tag:
            return '<' + tag + ' />'
        else:
            return '<' + tag + '></' + tag + '>'
        
    def feed(self, data):
        data = self._r_barebang.sub(r'&lt;!\1', data)
        data = self._r_bareamp.sub("&amp;", data)
        data = self._r_shorttag.sub(self._shorttag_replace, data) 
        if self.encoding and type(data) == type(u''):
            data = data.encode(self.encoding)
        sgmllib.SGMLParser.feed(self, data)

    def normalize_attrs(self, attrs):
        # utility method to be called by descendants
        attrs = [(k.lower(), v) for k, v in attrs]
        attrs = [(k, k in ('rel', 'type') and v.lower() or v) for k, v in attrs]
        return attrs

    def unknown_starttag(self, tag, attrs):
        # called for each start tag
        # attrs is a list of (attr, value) tuples
        # e.g. for <pre class='screen'>, tag='pre', attrs=[('class', 'screen')]
        if _debug: sys.stderr.write('_BaseHTMLProcessor, unknown_starttag, tag=%s\n' % tag)
        uattrs = []
        # thanks to Kevin Marks for this breathtaking hack to deal with (valid) high-bit attribute values in UTF-8 feeds
        for key, value in attrs:
            if type(value) != type(u''):
                value = unicode(value, self.encoding)
            uattrs.append((unicode(key, self.encoding), value))
        strattrs = u''.join([u' %s="%s"' % (key, value) for key, value in uattrs]).encode(self.encoding)
        if tag in self.elements_no_end_tag:
            self.pieces.append('<%(tag)s%(strattrs)s />' % locals())
        else:
            self.pieces.append('<%(tag)s%(strattrs)s>' % locals())

    def unknown_endtag(self, tag):
        # called for each end tag, e.g. for </pre>, tag will be 'pre'
        # Reconstruct the original end tag.
        if tag not in self.elements_no_end_tag:
            self.pieces.append("</%(tag)s>" % locals())

    def handle_charref(self, ref):
        # called for each character reference, e.g. for '&#160;', ref will be '160'
        # Reconstruct the original character reference.
        self.pieces.append('&#%(ref)s;' % locals())
        
    def handle_entityref(self, ref):
        # called for each entity reference, e.g. for '&copy;', ref will be 'copy'
        # Reconstruct the original entity reference.
        self.pieces.append('&%(ref)s;' % locals())

    def handle_data(self, text):
        # called for each block of plain text, i.e. outside of any tag and
        # not containing any character or entity references
        # Store the original text verbatim.
        if _debug: sys.stderr.write('_BaseHTMLProcessor, handle_text, text=%s\n' % text)
        self.pieces.append(text)
        
    def handle_comment(self, text):
        # called for each HTML comment, e.g. <!-- insert Javascript code here -->
        # Reconstruct the original comment.
        self.pieces.append('<!--%(text)s-->' % locals())
        
    def handle_pi(self, text):
        # called for each processing instruction, e.g. <?instruction>
        # Reconstruct original processing instruction.
        self.pieces.append('<?%(text)s>' % locals())

    def handle_decl(self, text):
        # called for the DOCTYPE, if present, e.g.
        # <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
        #     "http://www.w3.org/TR/html4/loose.dtd">
        # Reconstruct original DOCTYPE
        self.pieces.append('<!%(text)s>' % locals())
        
    _new_declname_match = re.compile(r'[a-zA-Z][-_.a-zA-Z0-9:]*\s*').match
    def _scan_name(self, i, declstartpos):
        rawdata = self.rawdata
        n = len(rawdata)
        if i == n:
            return None, -1
        m = self._new_declname_match(rawdata, i)
        if m:
            s = m.group()
            name = s.strip()
            if (i + len(s)) == n:
                return None, -1  # end of buffer
            return name.lower(), m.end()
        else:
            self.handle_data(rawdata)
#            self.updatepos(declstartpos, i)
            return None, -1

    def output(self):
        '''Return processed HTML as a single string'''
        return ''.join([str(p) for p in self.pieces])

class _HTMLSanitizer(_BaseHTMLProcessor):
    acceptable_elements = ['a', 'abbr', 'acronym', 'address', 'area', 'b', 'big',
      'blockquote', 'br', 'button', 'caption', 'center', 'cite', 'code', 'col', 
      'colgroup', 'dd', 'del', 'dfn', 'dir', 'div', 'dl', 'dt', 'em', 'fieldset',
      'font', 'form', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'hr', 'i', 'img', 'input',
      'ins', 'kbd', 'label', 'legend', 'li', 'map', 'menu', 'ol', 'optgroup', 
      'option', 'p', 'pre', 'q', 's', 'samp', 'select', 'small', 'span', 'strike',
      'strong', 'sub', 'sup', 'table', 'textarea', 'tbody', 'td', 'tfoot', 'th', 
      'thead', 'tr', 'tt', 'u', 'ul', 'var']
    
    acceptable_attributes = ['abbr', 'accept', 'accept-charset', 'accesskey',
      'action', 'align', 'alt', 'axis', 'border', 'cellpadding', 'cellspacing',
      'char', 'charoff', 'charset', 'checked', 'cite', 'class', 'clear', 'cols',
      'colspan', 'color', 'compact', 'coords', 'datetime', 'dir', 'disabled',
      'enctype', 'for', 'frame', 'headers', 'height', 'href', 'hreflang', 'hspace',
      'id', 'ismap', 'label', 'lang', 'longdesc', 'maxlength', 'media', 'method',
      'multiple', 'name', 'nohref', 'noshade', 'nowrap', 'prompt', 'readonly',
      'rel', 'rev', 'rows', 'rowspan', 'rules', 'scope', 'selected', 'shape', 'size',
      'span', 'src', 'start', 'summary', 'tabindex', 'target', 'title', 'type',
      'usemap', 'valign', 'value', 'vspace', 'width']

    ignorable_elements = ['script', 'applet', 'style']
            
    def reset(self):
        _BaseHTMLProcessor.reset(self)
        self.tag_stack = []
        self.ignore_level = 0

    def feed(self, data):
        _BaseHTMLProcessor.feed(self, data)
        while self.tag_stack:
            _BaseHTMLProcessor.unknown_endtag(self, self.tag_stack.pop())
        
    def unknown_starttag(self, tag, attrs):
        if tag in self.ignorable_elements:
            self.ignore_level += 1
            return
        
        if self.ignore_level:
            return
        
        if tag in self.acceptable_elements:
            attrs = self.normalize_attrs(attrs)
            attrs = [(key, value) for key, value in attrs if key in self.acceptable_attributes]
            if tag not in self.elements_no_end_tag:
                self.tag_stack.append(tag)
            _BaseHTMLProcessor.unknown_starttag(self, tag, attrs)
        
    def unknown_endtag(self, tag):
        if tag in self.ignorable_elements:
            self.ignore_level -= 1
            return
        
        if self.ignore_level:
            return
        
        if tag in self.acceptable_elements and tag not in self.elements_no_end_tag:
            match = False
            while self.tag_stack:
                top = self.tag_stack.pop()
                if top == tag:
                    match = True
                    break
                _BaseHTMLProcessor.unknown_endtag(self, top)

            if match:
                _BaseHTMLProcessor.unknown_endtag(self, tag)

    def handle_pi(self, text):
        pass

    def handle_decl(self, text):
        pass

    def handle_data(self, text):
        if not self.ignore_level:
            text = text.replace('<', '')
            _BaseHTMLProcessor.handle_data(self, text)

def HTML(htmlSource, encoding='utf8'):
    p = _HTMLSanitizer(encoding)
    p.feed(htmlSource)
    data = p.output()
    if TIDY_MARKUP:
        # loop through list of preferred Tidy interfaces looking for one that's installed,
        # then set up a common _tidy function to wrap the interface-specific API.
        _tidy = None
        for tidy_interface in PREFERRED_TIDY_INTERFACES:
            try:
                if tidy_interface == "uTidy":
                    from tidy import parseString as _utidy
                    def _tidy(data, **kwargs):
                        return str(_utidy(data, **kwargs))
                    break
                elif tidy_interface == "mxTidy":
                    from mx.Tidy import Tidy as _mxtidy
                    def _tidy(data, **kwargs):
                        nerrors, nwarnings, data, errordata = _mxtidy.tidy(data, **kwargs)
                        return data
                    break
            except:
                pass
        if _tidy:
            utf8 = type(data) == type(u'')
            if utf8:
                data = data.encode('utf-8')
            data = _tidy(data, output_xhtml=1, numeric_entities=1, wrap=0, char_encoding="utf8")
            if utf8:
                data = unicode(data, 'utf-8')
            if data.count('<body'):
                data = data.split('<body', 1)[1]
                if data.count('>'):
                    data = data.split('>', 1)[1]
            if data.count('</body'):
                data = data.split('</body', 1)[0]
    data = data.strip().replace('\r\n', '\n')
    return data

unicode_bom_map = {
  '\x00\x00\xfe\xff': 'utf-32be',
  '\xff\xfe\x00\x00': 'utf-32le',
  '\xfe\xff##': 'utf-16be',
  '\xff\xfe##': 'utf-16le',
  '\xef\bb\bf': 'utf-8'
}
xml_bom_map = {
  '\x00\x00\x00\x3c': 'utf-32be',
  '\x3c\x00\x00\x00': 'utf-32le',
  '\x00\x3c\x00\x3f': 'utf-16be',
  '\x3c\x00\x3f\x00': 'utf-16le',
  '\x3c\x3f\x78\x6d': 'utf-8', # or equivalent
  '\x4c\x6f\xa7\x94': 'ebcdic'
}

_ebcdic_to_ascii_map = None
def _ebcdic_to_ascii(s):
    global _ebcdic_to_ascii_map
    if not _ebcdic_to_ascii_map:
        emap = (
            0,1,2,3,156,9,134,127,151,141,142,11,12,13,14,15,
            16,17,18,19,157,133,8,135,24,25,146,143,28,29,30,31,
            128,129,130,131,132,10,23,27,136,137,138,139,140,5,6,7,
            144,145,22,147,148,149,150,4,152,153,154,155,20,21,158,26,
            32,160,161,162,163,164,165,166,167,168,91,46,60,40,43,33,
            38,169,170,171,172,173,174,175,176,177,93,36,42,41,59,94,
            45,47,178,179,180,181,182,183,184,185,124,44,37,95,62,63,
            186,187,188,189,190,191,192,193,194,96,58,35,64,39,61,34,
            195,97,98,99,100,101,102,103,104,105,196,197,198,199,200,201,
            202,106,107,108,109,110,111,112,113,114,203,204,205,206,207,208,
            209,126,115,116,117,118,119,120,121,122,210,211,212,213,214,215,
            216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,
            123,65,66,67,68,69,70,71,72,73,232,233,234,235,236,237,
            125,74,75,76,77,78,79,80,81,82,238,239,240,241,242,243,
            92,159,83,84,85,86,87,88,89,90,244,245,246,247,248,249,
            48,49,50,51,52,53,54,55,56,57,250,251,252,253,254,255
            )
        import string
        _ebcdic_to_ascii_map = string.maketrans( \
            ''.join(map(chr, range(256))), ''.join(map(chr, emap)))
    return s.translate(_ebcdic_to_ascii_map)

def _startswithbom(text, bom):
    for i, c in enumerate(bom):
        if c == '#':
            if text[i] == '\x00':
                return False
        else:
            if text[i] != c:
                return False
    return True

def _detectbom(text, bom_map=unicode_bom_map):
    for bom, encoding in bom_map.iteritems():
        if _startswithbom(text, bom):
            return encoding
    return None

def characters(text, isXML=False, guess=None):
    """
    Takes a string text of unknown encoding and tries to 
    provide a Unicode string for it.
    """
    _triedEncodings = []
    def tryEncoding(encoding):
        if encoding and encoding not in _triedEncodings:
            if encoding == 'ebcdic':
                return _ebcdic_to_ascii(text)
            try:
                return unicode(text, encoding)
            except UnicodeDecodeError:
                pass
            _triedEncodings.append(encoding)
    
    return (
      tryEncoding(guess) or 
      tryEncoding(_detectbom(text)) or 
      isXML and tryEncoding(_detectbom(text, xml_bom_map)) or
      tryEncoding(_chardet(text)) or
      tryEncoding('utf8') or
      tryEncoding('windows-1252') or
      tryEncoding('iso-8859-1'))
