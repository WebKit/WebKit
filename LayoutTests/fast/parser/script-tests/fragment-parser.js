description('Test that fragment parsing does not affect the host document.');

function parse(string, removeDocumentElement) {
    var iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    var doc = iframe.contentDocument;
    doc.documentElement.removeChild(doc.documentElement.firstChild);
    if (removeDocumentElement)
        doc.removeChild(doc.documentElement);

    var div = doc.createDocumentFragment().appendChild(doc.createElement("div"));
    div.innerHTML = string;
    document.body.removeChild(iframe);
    return [div.innerHTML, doc.documentElement ? doc.documentElement.outerHTML : "no document element"];
}

shouldBe("parse('<span><body bgcolor=red>')", "['<span></span>','<html><body></body></html>']");
shouldBe("parse('<span><html bgcolor=red>')", "['<span></span>','<html><body></body></html>']");
shouldBe("parse('<span><meta>')", "['<span></span>','<html><body></body></html>']");
shouldBe("parse('<span><base>')", "['<span></span>','<html><body></body></html>']");
shouldBe("parse('<html><script>')", "['','<html><body></body></html>']");
shouldBe("parse('<html><style>')", "['','<html><body></body></html>']");
shouldBe("parse('<html><meta>')", "['','<html><body></body></html>']");
shouldBe("parse('<html><link>')", "['','<html><body></body></html>']");
shouldBe("parse('<html><object>')", "['<object></object>','<html><body></body></html>']");
shouldBe("parse('<html><embed>')", "['<embed>','<html><body></body></html>']");

shouldBe("parse('<html><title>')", "['','<html><body></body></html>']");
shouldBe("parse('<html><isindex>')", "['<div><hr>This is a searchable index. Enter search keywords: <isindex type=\"khtml_isindex\"><hr></div>','<html><body></body></html>']");
shouldBe("parse('<html><base>')", "['','<html><body></body></html>']");
shouldBe("parse('<html><div>')", "['<div></div>','<html><body></body></html>']");
shouldBe("parse('<frameset>')", "['<frameset></frameset>','<html><body></body></html>']");
shouldBe("parse('<html>x', true)", "['x','no document element']");

var successfullyParsed = true;
