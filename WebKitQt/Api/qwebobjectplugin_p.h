#ifndef QWEBOBJECTPLUGIN_P_H
#define QWEBOBJECTPLUGIN_P_H

#include <qglobal.h>
#include <qwebobjectplugin.h>

/*
  FIXME: This is copied from qfactoryloader_p.h.
  Remove this once we made qfactoryloader public in Qt
*/
class QFactoryLoaderPrivate;

class Q_CORE_EXPORT QFactoryLoader : public QObject
{
    Q_OBJECT_FAKE
    Q_DECLARE_PRIVATE(QFactoryLoader)

public:
    QFactoryLoader(const char *iid,
                   const QStringList &paths = QStringList(),
                   const QString &suffix = QString(),
                   Qt::CaseSensitivity = Qt::CaseSensitive);
    ~QFactoryLoader();

    QStringList keys() const;
    QObject *instance(const QString &key) const;

};

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
    inline bool supportsMimeType(const QString &mimeType) const {
        return !nameForMimetype(mimeType).isEmpty();
    }

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
