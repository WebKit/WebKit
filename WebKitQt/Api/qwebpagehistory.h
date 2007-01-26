#ifndef QWEBPAGEHISTORY_H
#define QWEBPAGEHISTORY_H

#include <QUrl>
#include <QString>
#include <QIcon>
#include <QDateTime>
#include <QSharedData>

class QWebPage;

class QWebHistoryItemPrivate;
class QWebHistoryItem
{
public:
    ~QWebHistoryItem();

    QWebHistoryItem *parent() const;
    QList<QWebHistoryItem*> children() const;

    QUrl originalUrl() const;
    QUrl currentUrl() const;

    QString title() const;
    QDateTime lastVisited() const;

    QPixmap icon() const;

    QWebHistoryItem(QWebHistoryItemPrivate *priv);
private:
    friend class QWebPageHistory;
    friend class QWebPage;
    QExplicitlySharedDataPointer<QWebHistoryItemPrivate> d;
};

class QWebPageHistoryPrivate;
class QWebPageHistory
{
public:
    QWebPageHistory(const QWebPageHistory &other);
    ~QWebPageHistory();
    
    QList<QWebHistoryItem> items() const;
    QList<QWebHistoryItem> backItems(int maxItems) const;
    QList<QWebHistoryItem> forwardItems(int maxItems) const;

    void goBack();
    void goForward();
    void goToItem(QWebHistoryItem *item);

    QWebHistoryItem backItem() const;
    QWebHistoryItem currentItem() const;
    QWebHistoryItem forwardItem() const;
    QWebHistoryItem itemAtIndex(int i) const;

    
    QWebPageHistory(QWebPageHistoryPrivate *priv);
private:
    QExplicitlySharedDataPointer<QWebPageHistoryPrivate> d;
};

#endif
