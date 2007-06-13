#ifndef QWEBOBJECTPLUGIN_P_H
#define QWEBOBJECTPLUGIN_P_H

#include <qglobal.h>
#include <qwebobjectplugin.h>

#include <private/qfactoryloader_p.h>
class QWebFrame;

class QWebFactoryLoader : public QFactoryLoader
{
    Q_OBJECT
public:
    QWebFactoryLoader(const char *iid,
                      const QStringList &paths = QStringList(),
                      const QString &suffix = QString(),
                      Qt::CaseSensitivity = Qt::CaseSensitive);

    static QWebFactoryLoader *self();

    QStringList names() const { return keys(); }
    QStringList extensions() const;
    QString descriptionForName(const QString &key) const;
    QStringList mimetypesForName(const QString &key) const;
    QString nameForMimetype(const QString &mimeType) const;

    QString mimeTypeForExtension(const QString &extension);

    QObject *create(QWebFrame *frame,
                    const QUrl &url,
                    const QString &mimeType,
                    const QStringList &argumentNames,
                    const QStringList &argumentValues);

    struct MimeInfo {
        QString type;
        QStringList extensions;
    };
    struct Info {
        QString name;
        QString description;
        QList<MimeInfo> mimes;
    };
    QList<Info> m_pluginInfo;
};

#endif
