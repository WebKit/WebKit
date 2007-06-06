#ifndef QWEBOBJECTPLUGIN_H
#define QWEBOBJECTPLUGIN_H

#include <qwebkitglobal.h>
#include <QtCore/qplugin.h>
#include <QtCore/qfactoryinterface.h>

struct QWEBKIT_EXPORT QWebObjectPluginFactoryInterface : public QFactoryInterface
{
    virtual QObject *create(QWidget *parent,
                            const QString &mimeType,
                            const QStringList &argumentNames,
                            const QStringList &argumentValues) const = 0;
};

#define QWebObjectPluginFactoryInterface_iid "com.trolltech.Qt.QWebObjectPluginFactoryInterface"
Q_DECLARE_INTERFACE(QWebObjectPluginFactoryInterface, QWebObjectPluginFactoryInterface_iid)

class QWEBKIT_EXPORT QWebObjectPlugin : public QObject, public QWebObjectPluginFactoryInterface
{
    Q_OBJECT
    Q_INTERFACES(QWebObjectPluginFactoryInterface:QFactoryInterface)
public:
    explicit QWebObjectPlugin(QObject *parent = 0);
    virtual ~QWebObjectPlugin();

    virtual QStringList keys() const = 0;
    virtual QStringList extensionsForMimetype(const QString &mimeType) const;
    virtual QObject *create(QWidget *parent,
                            const QString &mimeType,
                            const QStringList &argumentNames,
                            const QStringList &argumentValues) const = 0;
};
    
#endif
