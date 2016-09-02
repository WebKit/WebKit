const DocumentTypes = [
    {
        name: 'document',
        create: function () { return Promise.resolve(document); },
        isOwner: true,
        hasBrowsingContext: true,
    },
    {
        name: 'document of a template element',
        create: function () {
            return new Promise(function (resolve) {
                var template = document.createElementNS('http://www.w3.org/1999/xhtml', 'template');
                var doc = template.content.ownerDocument;
                if (!doc.documentElement)
                    doc.appendChild(doc.createElement('html'));
                resolve(doc);
            });
        },
        hasBrowsingContext: false,
    },
    {
        name: 'new document',
        create: function () {
            return new Promise(function (resolve) {
                var doc = new Document();
                doc.appendChild(doc.createElement('html'));
                resolve(doc);
            });
        },
        hasBrowsingContext: false,
    },
    {
        name: 'cloned document',
        create: function () {
            return new Promise(function (resolve) {
                var doc = document.cloneNode(false);
                doc.appendChild(doc.createElement('html'));
                resolve(doc);
            });
        },
        hasBrowsingContext: false,
    },
    {
        name: 'document created by createHTMLDocument',
        create: function () {
            return Promise.resolve(document.implementation.createHTMLDocument());
        },
        hasBrowsingContext: false,
    },
    {
        name: 'HTML document created by createDocument',
        create: function () {
            return Promise.resolve(document.implementation.createDocument('http://www.w3.org/1999/xhtml', 'html', null));
        },
        hasBrowsingContext: false,
    },
    {
        name: 'document in an iframe',
        create: function () {
            return new Promise(function (resolve, reject) {
                var iframe = document.createElement('iframe');
                iframe.onload = function () { resolve(iframe.contentDocument); }
                iframe.onerror = function () { reject('Failed to load an empty iframe'); }
                document.body.appendChild(iframe);
            });
        },
        hasBrowsingContext: true,
    },
    {
        name: 'HTML document fetched by XHR',
        create: function () {
            return new Promise(function (resolve, reject) {
                var xhr = new XMLHttpRequest();
                xhr.open('GET', 'resources/empty-html-document.html');
                xhr.overrideMimeType('text/xml');
                xhr.onload = function () { resolve(xhr.responseXML); }
                xhr.onerror = function () { reject('Failed to fetch the document'); }
                xhr.send();
            });
        },
        hasBrowsingContext: false,
    }
];