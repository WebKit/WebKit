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
shouldBe("parse('<span><meta>')", "['<span><meta></span>','<html><body></body></html>']");
shouldBe("parse('<span><base>')", "['<span><base></span>','<html><body></body></html>']");
shouldBe("parse('<html><script>')", "['<script></script>','<html><body></body></html>']");
shouldBe("parse('<html><style>')", "['<style></style>','<html><body></body></html>']");
shouldBe("parse('<html><meta>')", "['<meta>','<html><body></body></html>']");
shouldBe("parse('<html><link>')", "['<link>','<html><body></body></html>']");
shouldBe("parse('<html><object>')", "['<object></object>','<html><body></body></html>']");
shouldBe("parse('<html><embed>')", "['<embed>','<html><body></body></html>']");

shouldBe("parse('<html><title>')", "['<title></title>','<html><body></body></html>']");
shouldBe("parse('<html><isindex>')", "['<form><hr><label>This is a searchable index. Enter search keywords: <input name=\"isindex\"></label><hr></form>','<html><body></body></html>']");
shouldBe("parse('<html><base>')", "['','<html><body></body></html>']");
shouldBe("parse('<html><div>')", "['<div></div>','<html><body></body></html>']");
shouldBe("parse('<frameset>')", "['','<html><body></body></html>']");
shouldBe("parse('<html>x', true)", "['x','no document element']");
