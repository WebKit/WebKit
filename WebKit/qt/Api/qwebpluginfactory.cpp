#include "config.h"
#include "qwebpluginfactory.h"

/*!
    \class QWebPluginFactory
    \since 4.4
    \brief The QWebPluginFactory creates plugins to be embedded into web pages.

    QWebPluginFactory is a factory for creating plugins for QWebPage. A plugin factory can be
    installed on a QWebPage using QWebPage::setPluginFactory().

    You can provide a QWebPluginFactory by implementing the plugins() and the create() method. For
    plugins() it is necessary to describe the plugins the factory can create, including a
    description and the supported mime types. The mime types are matched with the mime type
    specified in the HTML object tag. The create() method is called if the requested mime type is
    supported. The implementation has to return a new instance of the plugin requested for the given
    mime type and the specified url.

    \note The plugin factory is only used if plugins are enabled through QWebSettings.
*/


/*!
    \class QWebPluginFactory::Plugin
    \since 4.4
    \brief the QWebPluginFactory::Plugin structure describes the properties of a plugin a QWebPluginFactory can create.
*/

/*!
    \variable QWebPluginFactory::Plugin::name
    The name of the plugin.
*/

/*!
    \variable QWebPluginFactory::Plugin::description
    The description of the plugin.
*/

/*!
    \variable QWebPluginFactory::Plugin::mimeTypes
    The list of mime types supported by the plugin.
*/

/*!
    \class QWebPluginFactory::MimeType
    \brief The QWebPluginFactory::MimeType structure describes a mime type supported by a plugin.
*/

/*!
    \variable QWebPluginFactory::MimeType::name
    The name of the mime type.
*/

/*!
    \variable QWebPluginFactory::MimeType::description
    The description of the mime type.
*/

/*!
    \variable QWebPluginFactory::MimeType::fileExtensions
    The list of file extensions that are used by this mime type.
*/

/*!
    Constructs a QWebPluginFactory with parent \a parent.
*/
QWebPluginFactory::QWebPluginFactory(QObject *parent)
    : QObject(parent)
{
}

/*!
    Destructor.
*/
QWebPluginFactory::~QWebPluginFactory()
{
}

/*!
    \fn QList<Plugin> QWebPluginFactory::plugins() const = 0

    Implemented in subclasses to return a list of supported plugins the factory can create.
*/

/*!
    This function is called to refresh the list of supported plugins. It may be called after a new plugin
    has been installed in the system.
*/
void QWebPluginFactory::refreshPlugins()
{
}

/*!
    \fn QObject *QWebPluginFactory::create(const QString &mimeType, const QUrl &url, const QStringList &argumentNames, const QStringList &argumentValues) const = 0

    Implemented in subclasses to create a new plugin that can display content of type \a mimeType.
    The url of the content is provided in \a url.

    The HTML object element can provide parameters through the param tag. The name and the value
    attributes of these tags are provided in the \a argumentNames and \a argumentValues string lists.

    For example:

    \code
    <object type="application/x-pdf" data="http://www.trolltech.com/document.pdf" width="500" height="400">
        <param name="showTableOfContents" value="true" />
        <param name="hideThumbnails" value="false" />
    </object>
    \endcode

    The above object element will result in a call to create() with the following arguments:
    \table
    \header \o Parameter
            \o Value
    \row    \o mimeType
            \o "application/x-pdf"
    \row    \o url
            \o "http://www.trolltech.com/document.pdf"
    \row    \o argumentNames
            \o "showTableOfContents" "hideThumbnails"
    \row    \o argumentVaues
            \o "true" "false"
    \endtable
*/

/*!
    \enum QWebPluginFactory::Extension

    This enum describes the types of extensions that the plugin factory can support. Before using these extensions, you
    should verify that the extension is supported by calling supportsExtension().

    Currently there are no extensions.
*/

/*!
    \class QWebPluginFactory::ExtensionOption
    \since 4.4
    \brief The ExtensionOption class provides an extended input argument to QWebPluginFactory's extension support.

    \sa QWebPluginFactory::extension()
*/

/*!
    \class QWebPluginFactory::ExtensionReturn
    \since 4.4
    \brief The ExtensionOption class provides an extended output argument to QWebPluginFactory's extension support.

    \sa QWebPluginFactory::extension()
*/

/*!
    This virtual function can be reimplemented in a QWebPluginFactory subclass to provide support for extensions. The \a option
    argument is provided as input to the extension; the output results can be stored in \a output.

    The behaviour of this function is determined by \a extension.

    You can call supportsExtension() to check if an extension is supported by the factory.

    By default, no extensions are supported, and this function returns false.

    \sa supportsExtension(), Extension
*/
bool QWebPluginFactory::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    Q_UNUSED(extension)
    Q_UNUSED(option)
    Q_UNUSED(output)
    return false;
}

/*!
    This virtual function returns true if the plugin factory supports \a extension; otherwise false is returned.

    \sa extension()
*/
bool QWebPluginFactory::supportsExtension(Extension extension) const
{
    Q_UNUSED(extension)
    return false;
}
