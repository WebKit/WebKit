
void wrapInFunction()
{

//! [0]
    // ...
    QWebPage *page = new QWebPage;
    // ...

    QWebInspector *inspector = new QWebInspector;
    inspector->setPage(page);

    connect(page, SIGNAL(webInspectorTriggered(QWebElement)), inspector, SLOT(show()));
//! [0]

}

