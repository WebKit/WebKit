// programm to test the CSS implementation

#include <kapp.h>
#include <kdebug.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "css/css_stylesheetimpl.h"
#include "dom/dom_string.h"
#include "css/cssstyleselector.h"
#include "html/html_documentimpl.h"
#include "html/html_elementimpl.h"
#include "html/html_blockimpl.h"
using namespace DOM;
using namespace khtml;

int main(int argc, char *argv[])
{
    KApplication a(argc, argv, "testcss");

    char buf[40000];
    int fd;

    if(argc == 2)
	fd = open(argv[1], O_RDONLY);
    else
	fd = open("/home/kde/test.css", O_RDONLY);
	
    if (fd < 0)
    {
        kdDebug( 6000 ) << "Couldn't open file" << endl;
        return 0;
    }

    int len = read(fd, buf, 40000);

    DOM::DOMString str(buf);

    close(fd);

    DOM::CSSStyleSheetImpl *sheet = new DOM::CSSStyleSheetImpl((DOM::CSSStyleSheetImpl *)0);
    sheet->parseString( str );
#if 0
    kdDebug( 6000 ) << "==============> finished parsing <==================" << endl;

    CSSStyleSelector *selector = new CSSStyleSelector(sheet);

    kdDebug( 6000 ) << "==============> finished creation of Selector <==================" << endl;

    HTMLDocumentImpl *doc;
    HTMLElementImpl *e = new HTMLParagraphElementImpl(doc);
    e->setAttribute(DOMString("id"), DOMString("myid"));
    e->setAttribute(DOMString("class"), DOMString("c myclass anotherclass"));

    selector->styleForElement(e);
#endif
    return 0;
}
