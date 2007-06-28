/*
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <qwebpage.h>
#include <qwebframe.h>
#include <qwebsettings.h>

#include <QtGui>
#include <QDebug>



class InfoWidget :public QProgressBar {
    Q_OBJECT
public:
    InfoWidget(QWidget *parent)
        : QProgressBar(parent), m_progress(0)
    {
        setMinimum(0);
        setMaximum(100);
    }
    QSize sizeHint() const
    {
        QSize size(100, 20);
        return size;
    }
public slots:
    void startLoad()
    {
        setValue(m_progress);
        show();
    }
    void changeLoad(int change)
    {
        m_progress = change;
        setValue(change);
    }
    void endLoad()
    {
        QTimer::singleShot(1000, this, SLOT(hide()));
        m_progress = 0;
    }

protected:
    int m_progress;
};

class HoverLabel : public QWidget {
    Q_OBJECT
public:
    HoverLabel(QWidget *parent=0)
        : QWidget(parent),
          m_animating(false),
          m_percent(0)
    {
        m_timer.setInterval(1000/30);
        m_hideTimer.setInterval(500);
        m_hideTimer.setSingleShot(true);
        connect(&m_timer, SIGNAL(timeout()),
                this, SLOT(update()));
        connect(&m_hideTimer, SIGNAL(timeout()),
                this, SLOT(hide()));
    }

public slots:
    void setHoverLink(const QString &link) {
        m_link = link;
        if (m_link.isEmpty()) {
            m_hideTimer.start();
        } else {
            m_hideTimer.stop();
            m_oldSize = m_newSize;
            m_newSize = sizeForFont();
            resetAnimation();
            updateSize();
            show();
            repaint();
        }
    }
    QSize sizeForFont() const {
        QFont f = font();
        QFontMetrics fm(f);
        return QSize(fm.width(m_link) + 10, fm.height() + 6);
    }
    QSize sizeHint() const {
        if (!m_animating)
            return sizeForFont();
        else
            return (m_newSize.width() > m_oldSize.width()) ? m_newSize : m_oldSize;
    }
    void updateSize() {
        QRect r = geometry();
        QSize newSize = sizeHint();
        r = QRect(r.x(), r.y(), newSize.width(), newSize.height());
        setGeometry(r);
    }
    void resetAnimation() {
        m_animating = true;
        m_percent = 0;
        if (!m_timer.isActive())
            m_timer.start();
    }
protected:
    void paintEvent(QPaintEvent *e) {
        QPainter p(this);
        p.setClipRect(e->rect());
        p.setPen(QPen(Qt::black, 1));
        QLinearGradient gradient(rect().topLeft(), rect().bottomLeft());
        gradient.setColorAt(0, QColor(255, 255, 255, 220));
        gradient.setColorAt(1, QColor(193, 193, 193, 220));
        p.setBrush(QBrush(gradient));
        QSize size;
        {
            //draw a nicely rounded corner rectangle. to avoid unwanted
            // borders we move the coordinates outsize the our clip region
            size = interpolate(m_oldSize, m_newSize, m_percent);
            QRect r(-1, 0, size.width(), size.height()+2);
            const int roundness = 20;
            QPainterPath path;
            path.moveTo(r.x(), r.y());
            path.lineTo(r.topRight().x()-roundness, r.topRight().y());
            path.cubicTo(r.topRight().x(), r.topRight().y(),
                         r.topRight().x(), r.topRight().y(),
                         r.topRight().x(), r.topRight().y() + roundness);
            path.lineTo(r.bottomRight());
            path.lineTo(r.bottomLeft());
            path.closeSubpath();
            p.setRenderHint(QPainter::Antialiasing);
            p.drawPath(path);
        }
        if (m_animating) {
            if (qFuzzyCompare(m_percent, 1)) {
                m_animating = false;
                m_percent = 0;
                m_timer.stop();
            } else {
                m_percent += 0.1;
                if (m_percent >= 0.99) {
                    m_percent = 1;
                }
            }
        }

        QString txt;
        QFontMetrics fm(fontMetrics());
        txt = fm.elidedText(m_link, Qt::ElideRight, size.width()-5);
        p.drawText(5, height()-6, txt);
    }

private:
    QSize interpolate(const QSize &src, const QSize &dst, qreal percent) {
        int widthDiff  = int((dst.width() - src.width())  * percent);
        int heightDiff = int((dst.height() - src.height()) * percent);
        return QSize(src.width()  + widthDiff,
                     src.height() + heightDiff);
    }
    QString m_link;
    bool    m_animating;
    QTimer  m_timer;
    QTimer  m_hideTimer;
    QSize   m_oldSize;
    QSize   m_newSize;
    qreal   m_percent;
};

class SearchEdit;

class ClearButton : public QPushButton {
    Q_OBJECT
public:
    ClearButton(QWidget *w)
        : QPushButton(w)
    {
        setMinimumSize(24, 24);
        setFixedSize(24, 24);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    void paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event);
        QPainter painter(this);
        int height = parentWidget()->geometry().height();
        int width = height; //parentWidget()->geometry().width();

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::lightGray);
        painter.setBrush(isDown() ?
                         QColor(140, 140, 190) :
                         underMouse() ? QColor(220, 220, 255) : QColor(200, 200, 230)
            );
        painter.drawEllipse(4, 4, width - 8, height - 8);
        painter.setPen(Qt::white);
        int border = 8;
        painter.drawLine(border, border, width - border, height - border);
        painter.drawLine(border, height - border, width - border, border);
    }
};

class SearchEdit : public QLineEdit
{
    Q_OBJECT
public:
    SearchEdit(const QString &str, QWidget *parent = 0)
        : QLineEdit(str, parent)
    {
        setMinimumSize(QSize(400, 24));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        setStyleSheet(":enabled { padding-right: 27px }");
        clearButton = new ClearButton(this);
        clearButton->setGeometry(QRect(geometry().right() - 27,
                                       geometry().top() - 2,
                                       geometry().right(), geometry().bottom()));
        clearButton->setVisible(true);
#ifndef QT_NO_CURSOR
        clearButton->setCursor(Qt::ArrowCursor);
#endif
#ifndef QT_NO_TOOLTIP
        clearButton->setToolTip("Clear");
#endif
        connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    }
    ~SearchEdit() { }
protected:
    virtual void paintEvent(QPaintEvent *e) {
        QLineEdit::paintEvent(e);
        if(text().isEmpty())
            clearButton->setVisible(false);
        else
            clearButton->setVisible(true);
    }
    virtual void resizeEvent(QResizeEvent *) {
        clearButton->setParent(this);
        clearButton->setGeometry(QRect(width()-27,
                                       0,
                                       24, 24));
    }
    virtual void moveEvent(QMoveEvent *) {
        clearButton->setParent(this);
        clearButton->setGeometry(QRect(width()-27, 1,
                                       24, 24));
    }

    QPushButton *clearButton;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(const QUrl &url)
    {
        page = new QWebPage(this);
        InfoWidget *info = new InfoWidget(page);
        info->setGeometry(20, 20, info->sizeHint().width(),
                          info->sizeHint().height());
        connect(page, SIGNAL(loadStarted(QWebFrame*)),
                info, SLOT(startLoad()));
        connect(page, SIGNAL(loadProgressChanged(int)),
                info, SLOT(changeLoad(int)));
        connect(page, SIGNAL(loadFinished(QWebFrame*)),
                info, SLOT(endLoad()));
        connect(page, SIGNAL(loadFinished(QWebFrame*)),
                this, SLOT(loadFinished()));
        connect(page, SIGNAL(titleChanged(const QString&)),
                this, SLOT(setWindowTitle(const QString&)));
        connect(page, SIGNAL(hoveringOverLink(const QString&, const QString&)),
                this, SLOT(showLinkHover(const QString&, const QString&)));


        setCentralWidget(page);

        QToolBar *bar = addToolBar("Navigation");
        urlEdit = new SearchEdit(url.toString());
        connect(urlEdit, SIGNAL(returnPressed()),
                SLOT(changeLocation()));
        bar->addAction("Go back", page, SLOT(goBack()));
        bar->addAction("Stop", page, SLOT(stop()));
        bar->addAction("Go forward", page, SLOT(goForward()));
        bar->addWidget(urlEdit);

        hoverLabel = new HoverLabel(this);
        hoverLabel->hide();

        page->open(url);

        info->raise();
    }
protected slots:
    void changeLocation()
    {
        QUrl url(urlEdit->text());
        page->open(url);
    }
    void loadFinished()
    {
        urlEdit->setText(page->url().toString());
    }
    void showLinkHover(const QString &link, const QString &toolTip)
    {
        //statusBar()->showMessage(link);
        hoverLabel->setHoverLink(link);
#ifndef QT_NO_TOOLTIP
        if (!toolTip.isEmpty())
            QToolTip::showText(QCursor::pos(), toolTip);
#endif
    }
protected:
    void resizeEvent(QResizeEvent *) {
        QSize hoverSize = hoverLabel->sizeHint();
        hoverLabel->setGeometry(0, height()-hoverSize.height(),
                                300, hoverSize.height());
    }
private:
    QWebPage *page;
    QLineEdit *urlEdit;
    HoverLabel *hoverLabel;
};

#include "main.moc"

int main(int argc, char **argv)
{
    QString url = QString("%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));
    QApplication app(argc, argv);

    QWebSettings settings = QWebSettings::global();
    settings.setAttribute(QWebSettings::PluginsEnabled);
    QWebSettings::setGlobal(settings);

    const QStringList args = app.arguments();
    if (args.count() > 1)
        url = args.at(1);

    MainWindow window(url);
    window.show();

    return app.exec();
}
