#include "qwebobjectplugin_p.h"
#include <qcoreapplication.h>
    
#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QWebFactoryLoader, loader,
                          (QWebObjectPluginFactoryInterface_iid, QCoreApplication::libraryPaths(), QLatin1String("/webplugins")))
#endif


QWebFactoryLoader::QWebFactoryLoader(const char *iid, const QStringList &paths, const QString &suffix, Qt::CaseSensitivity)
    : QFactoryLoader(iid, paths, suffix)
{
    QStringList plugins = keys();
    foreach(QString k, plugins) {
        QWebObjectPlugin *plugin = qobject_cast<QWebObjectPlugin *>(instance(k));
        if (!plugin)
            continue;
        QStringList extensions = plugin->extensionsForMimetype(k);
        foreach(QString ext, extensions) {
            m_extensions.append(ext);
            m_mimeTypesForExtension.append(k);
        }
    }
}

QWebFactoryLoader *QWebFactoryLoader::self()
{
    return loader();
}
    
QString QWebFactoryLoader::mimeTypeForExtension(const QString &extension)
{
    int idx = m_extensions.indexOf(extension);
    if (idx > 0)
        return m_mimeTypesForExtension.at(idx);
    return QString();
}

QObject *QWebFactoryLoader::create(QWidget *parent,
                                   const QString &mimeType,
                                   const QStringList &argumentNames,
                                   const QStringList &argumentValues)
{
    QWebObjectPlugin *plugin = qobject_cast<QWebObjectPlugin *>(instance(mimeType));
    if (!plugin)
        return 0;
    return plugin->create(parent, mimeType, argumentNames, argumentValues);
}



/*! \class QWebObjectPlugin

  This class is a plugin for the HTML object tag. It can be used to embed arbitrary content in a web page.
*/


QWebObjectPlugin::QWebObjectPlugin(QObject *parent)
    : QObject(parent)
{
}

QWebObjectPlugin::~QWebObjectPlugin()
{
}

/*!
  \fn QStringList QWebObjectPlugin::keys() const

  The keys are the mimetypes the plugin can handle
*/

/*!
  \fn QStringList QWebObjectPlugin::extensionsForMimetype() const

  Should return a list of extensions that are recognised to match the \a mimeType.
*/
QStringList QWebObjectPlugin::extensionsForMimetype(const QString &mimeType) const
{
    return QStringList();
}

/*!
  \fn QObject *QWebObjectPlugin::create(QWidget *parent, const QString &mimeType, const QStringList &argumentNames, const QStringList &argumentValues) const

  Creates a QObject with \a parent to handle \a mimeType. \a argumentNames and \a argumentValues are a set of key-value pairs passed directly
  from the &lt;param&gt; elements contained in the HTML object tag.
*/

