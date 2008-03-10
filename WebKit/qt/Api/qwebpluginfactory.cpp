#include "config.h"
#include "qwebpluginfactory.h"

QWebPluginFactory::QWebPluginFactory(QObject *parent)
    : QObject(parent)
{
}

QWebPluginFactory::~QWebPluginFactory()
{
}

void QWebPluginFactory::refreshPlugins()
{
}

bool QWebPluginFactory::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
}

bool QWebPluginFactory::supportsExtension(Extension extension) const
{
}
