
void wrapInFunction()
{

//! [0]
    // ...
    QWebPage *page = new QWebPage;
    // ...

    QWebInspector *inspector = new QWebInspector;
    inspector->setPage(page);

    connect(page, SIGNAL(webInspectorTriggered(const QWebElement&)), inspector, SLOT(show()));
//! [0]

}

