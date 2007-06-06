#ifndef QWEBOBJECTPLUGIN_P_H
#define QWEBOBJECTPLUGIN_P_H

#include <qglobal.h>
#include <qwebobjectplugin.h>

#include <private/qfactoryloader_p.h>

class QWebFactoryLoader : public QFactoryLoader
{
    Q_OBJECT
public:
    QWebFactoryLoader(const char *iid,
                      const QStringList &paths = QStringList(),
                      const QString &suffix = QString(),
                      Qt::CaseSensitivity = Qt::CaseSensitive);

    static QWebFactoryLoader *self();

    QStringList mimeTypes() { return keys(); }
    QStringList extensions() { return m_extensions; }

    QString mimeTypeForExtension(const QString &extension);
    bool supportsMimeType(const QString &mimeType) { return keys().contains(mimeType); }
    
    QObject *create(QWidget *parent,
                    const QString &mimeType,
                    const QStringList &argumentNames,
                    const QStringList &argumentValues);

private:
    QStringList m_extensions;
    QStringList m_mimeTypesForExtension;
};

#endif
