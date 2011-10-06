/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "qdesktopwebview.h"

#include "UtilsQt.h"
#include "qdesktopwebview_p.h"
#include "qweberror.h"
#include <QFileDialog>
#include <QGraphicsSceneEvent>
#include <QGraphicsSceneResizeEvent>
#include <QStyleOptionGraphicsItem>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qsgcanvas.h>
#include <QtDeclarative/qsgevent.h>
#include <QtDeclarative/qsgitem.h>
#include <QtDeclarative/qsgview.h>
#include <QtGui/QCursor>
#include <QtGui/QDrag>
#include <QtGui/QFocusEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QHoverEvent>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QTouchEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <WKOpenPanelResultListener.h>

QDesktopWebViewPrivate::QDesktopWebViewPrivate(QDesktopWebView* q, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : q(q)
    , page(this, contextRef, pageGroupRef)
    , isCrashed(false)
    , navigationController(0)
{
}

void QDesktopWebViewPrivate::enableMouseEvents()
{
    q->setAcceptedMouseButtons(Qt::MouseButtonMask);
    q->setAcceptHoverEvents(true);
}

void QDesktopWebViewPrivate::disableMouseEvents()
{
    q->setAcceptedMouseButtons(Qt::NoButton);
    q->setAcceptHoverEvents(false);
}

void QDesktopWebViewPrivate::setViewNeedsDisplay(const QRect& invalidatedArea)
{
    q->update(invalidatedArea);
}

QSize QDesktopWebViewPrivate::drawingAreaSize()
{
    return QSize(q->width(), q->height());
}

void QDesktopWebViewPrivate::contentSizeChanged(const QSize&)
{
}

bool QDesktopWebViewPrivate::isActive()
{
    // FIXME: The scene graph did not have the concept of being active or not when this was written.
    return true;
}

bool QDesktopWebViewPrivate::hasFocus()
{
    return q->hasFocus();
}

bool QDesktopWebViewPrivate::isVisible()
{
    return q->isVisible();
}

void QDesktopWebViewPrivate::startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData* data, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction* dropAction)
{
    QWindow* window = q->canvas();
    if (!window)
        return;

    QDrag* drag = new QDrag(window);
    drag->setPixmap(QPixmap::fromImage(dragImage));
    drag->setMimeData(data);
    *dropAction = drag->exec(supportedDropActions);
    *globalPosition = QCursor::pos();
    *clientPosition = window->mapFromGlobal(*globalPosition);
}

void QDesktopWebViewPrivate::didReceiveViewportArguments(const WebCore::ViewportArguments&)
{
    // This feature is only used by QTouchWebView.
}

void QDesktopWebViewPrivate::didFindZoomableArea(const QPoint&, const QRect&)
{
    // This feature is only used by QTouchWebView.
}

void QDesktopWebViewPrivate::didChangeUrl(const QUrl& url)
{
    emit q->urlChanged(url);
}

void QDesktopWebViewPrivate::didChangeTitle(const QString& newTitle)
{
    emit q->titleChanged(newTitle);
}

void QDesktopWebViewPrivate::didChangeToolTip(const QString& newToolTip)
{
    // FIXME: Add a proper implementation when Qt 5 supports tooltip.
}

void QDesktopWebViewPrivate::didChangeStatusText(const QString& newMessage)
{
    emit q->statusBarMessageChanged(newMessage);
}

void QDesktopWebViewPrivate::didChangeCursor(const QCursor& newCursor)
{
    // FIXME: This is a temporary fix until we get cursor support in QML items.
    QGuiApplication::setOverrideCursor(newCursor);
}

void QDesktopWebViewPrivate::loadDidBegin()
{
    emit q->loadStarted();
}

void QDesktopWebViewPrivate::loadDidCommit()
{
    // Not used for QDesktopWebView.
}

void QDesktopWebViewPrivate::loadDidSucceed()
{
    emit q->loadSucceeded();
}

void QDesktopWebViewPrivate::loadDidFail(const QWebError& error)
{
    emit q->loadFailed(static_cast<QDesktopWebView::ErrorType>(error.type()), error.errorCode(), error.url());
}

void QDesktopWebViewPrivate::didChangeLoadProgress(int percentageLoaded)
{
    emit q->loadProgressChanged(percentageLoaded);
}

void QDesktopWebViewPrivate::showContextMenu(QSharedPointer<QMenu> menu)
{
    // Remove the active menu in case this function is called twice.
    if (activeMenu)
        activeMenu->hide();

    if (menu->isEmpty())
        return;

    QWindow* window = q->canvas();
    if (!window)
        return;

    activeMenu = menu;

    activeMenu->window()->winId(); // Ensure that the menu has a window
    Q_ASSERT(activeMenu->window()->windowHandle());
    activeMenu->window()->windowHandle()->setTransientParent(window);

    QPoint menuPositionInScene = q->mapToScene(menu->pos()).toPoint();
    menu->exec(window->mapToGlobal(menuPositionInScene));
    // The last function to get out of exec() clear the local copy.
    if (activeMenu == menu)
        activeMenu.clear();
}

void QDesktopWebViewPrivate::hideContextMenu()
{
    if (activeMenu)
        activeMenu->hide();
}

void QDesktopWebViewPrivate::runJavaScriptAlert(const QString& alertText)
{
#ifndef QT_NO_MESSAGEBOX
    const QString title = tr("JavaScript Alert - %1").arg(q->url().host());
    disableMouseEvents();
    QMessageBox::information(0, title, escapeHtml(alertText), QMessageBox::Ok);
    enableMouseEvents();
#else
    Q_UNUSED(alertText);
#endif
}

bool QDesktopWebViewPrivate::runJavaScriptConfirm(const QString& message)
{
    bool result = true;
#ifndef QT_NO_MESSAGEBOX
    const QString title = tr("JavaScript Confirm - %1").arg(q->url().host());
    disableMouseEvents();
    result = QMessageBox::Yes == QMessageBox::information(0, title, escapeHtml(message), QMessageBox::Yes, QMessageBox::No);
    enableMouseEvents();
#else
    Q_UNUSED(message);
#endif
    return result;
}

QString QDesktopWebViewPrivate::runJavaScriptPrompt(const QString& message, const QString& defaultValue, bool& ok)
{
#ifndef QT_NO_INPUTDIALOG
    const QString title = tr("JavaScript Prompt - %1").arg(q->url().host());
    disableMouseEvents();
    QString result = QInputDialog::getText(0, title, escapeHtml(message), QLineEdit::Normal, defaultValue, &ok);
    enableMouseEvents();
    return result;
#else
    Q_UNUSED(message);
    return defaultValue;
#endif
}

QDesktopWebView::QDesktopWebView(QSGItem* parent)
    : QSGPaintedItem(parent)
    , d(new QDesktopWebViewPrivate(this))
{
    init();
}

QDesktopWebView::QDesktopWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef, QSGItem* parent)
    : QSGPaintedItem(parent)
    , d(new QDesktopWebViewPrivate(this, contextRef, pageGroupRef))
{
    init();
}

void QDesktopWebView::init()
{
    d->enableMouseEvents();
}

QDesktopWebView::~QDesktopWebView()
{
    delete d;
}

void QDesktopWebView::load(const QUrl& url)
{
    d->page.load(url);
}

QUrl QDesktopWebView::url() const
{
    return d->page.url();
}

int QDesktopWebView::loadProgress() const
{
    return d->page.loadProgress();
}

QString QDesktopWebView::title() const
{
    return d->page.title();
}

QWebNavigationController* QDesktopWebView::navigationController() const
{
    if (!d->navigationController)
        d->navigationController = new QWebNavigationController(&d->page);
    return d->navigationController;
}

static void paintCrashedPage(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, Qt::gray);
    painter->drawText(rect, Qt::AlignCenter, QLatin1String(":("));
}

void QDesktopWebView::keyPressEvent(QKeyEvent* event)
{
    this->event(event);
}

void QDesktopWebView::keyReleaseEvent(QKeyEvent* event)
{
    this->event(event);
}

void QDesktopWebView::inputMethodEvent(QInputMethodEvent* event)
{
    this->event(event);
}

void QDesktopWebView::focusInEvent(QFocusEvent* event)
{
    this->event(event);
}

void QDesktopWebView::focusOutEvent(QFocusEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mousePressEvent(QMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mouseMoveEvent(QMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mouseReleaseEvent(QMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::mouseDoubleClickEvent(QMouseEvent* event)
{
    this->event(event);
}

void QDesktopWebView::wheelEvent(QWheelEvent* event)
{
    this->event(event);
}

void QDesktopWebView::touchEvent(QTouchEvent* event)
{
    this->event(event);
}

void QDesktopWebView::hoverEnterEvent(QHoverEvent* event)
{
    this->event(event);
}

void QDesktopWebView::hoverMoveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QDesktopWebView::hoverLeaveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragMoveEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragEnterEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragExitEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::dragDropEvent(QSGDragEvent* event)
{
    this->event(event);
}

void QDesktopWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QSGPaintedItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->page.setDrawingAreaSize(newGeometry.size().toSize());
}

void QDesktopWebView::paint(QPainter* painter)
{
    const QRectF rect = boundingRect();
    if (d->isCrashed) {
        paintCrashedPage(painter, rect);
        return;
    }

    d->page.paint(painter, rect.toAlignedRect());
}

bool QDesktopWebView::event(QEvent* ev)
{
    if (d->page.handleEvent(ev))
        return true;
    if (ev->type() == QEvent::InputMethod)
        return false; // This is necessary to avoid an endless loop in connection with QSGItem::event().
    return QSGItem::event(ev);
}

WKPageRef QDesktopWebView::pageRef() const
{
    return d->page.pageRef();
}

void QDesktopWebViewPrivate::processDidCrash()
{
    isCrashed = true;
    q->update();
}

void QDesktopWebViewPrivate::didRelaunchProcess()
{
    isCrashed = false;
    q->update();
}

QJSEngine* QDesktopWebViewPrivate::engine()
{
    QSGView* view = qobject_cast<QSGView*>(q->canvas());
    if (view)
        return view->engine();
    return 0;
}

void QDesktopWebViewPrivate::chooseFiles(WKOpenPanelResultListenerRef listenerRef, const QStringList& selectedFileNames, ViewInterface::FileChooserType type)
{
#ifndef QT_NO_FILEDIALOG
    openPanelResultListener = listenerRef;

    // Qt does not support multiple files suggestion, so we get just the first suggestion.
    QString selectedFileName;
    if (!selectedFileNames.isEmpty())
        selectedFileName = selectedFileNames.at(0);

    Q_ASSERT(!fileDialog);

    QWindow* window = q->canvas();
    if (!window)
        return;

    fileDialog = new QFileDialog(0, QString(), selectedFileName);
    fileDialog->window()->winId(); // Ensure that the dialog has a window
    Q_ASSERT(fileDialog->window()->windowHandle());
    fileDialog->window()->windowHandle()->setTransientParent(window);

    fileDialog->open(this, SLOT(onOpenPanelFilesSelected()));

    connect(fileDialog, SIGNAL(finished(int)), this, SLOT(onOpenPanelFinished(int)));
#endif
}

void QDesktopWebViewPrivate::onOpenPanelFilesSelected()
{
    const QStringList fileList = fileDialog->selectedFiles();
    Vector<RefPtr<APIObject> > wkFiles(fileList.size());

    for (unsigned i = 0; i < fileList.size(); ++i)
        wkFiles[i] = WebURL::create(QUrl::fromLocalFile(fileList.at(i)).toString());

    WKOpenPanelResultListenerChooseFiles(openPanelResultListener, toAPI(ImmutableArray::adopt(wkFiles).leakRef()));
}

void QDesktopWebViewPrivate::onOpenPanelFinished(int result)
{
    if (result == QDialog::Rejected)
        WKOpenPanelResultListenerCancel(openPanelResultListener);

    fileDialog->deleteLater();
    fileDialog = 0;
}

void QDesktopWebViewPrivate::didMouseMoveOverElement(const QUrl& linkURL, const QString& linkTitle)
{
    if (linkURL == lastHoveredURL && linkTitle == lastHoveredTitle)
        return;
    lastHoveredURL = linkURL;
    lastHoveredTitle = linkTitle;
    emit q->linkHovered(lastHoveredURL, lastHoveredTitle);
}

static PolicyInterface::PolicyAction toPolicyAction(QDesktopWebView::NavigationPolicy policy)
{
    switch (policy) {
    case QDesktopWebView::UsePolicy:
        return PolicyInterface::Use;
    case QDesktopWebView::DownloadPolicy:
        return PolicyInterface::Download;
    case QDesktopWebView::IgnorePolicy:
        return PolicyInterface::Ignore;
    }
    ASSERT_NOT_REACHED();
    return PolicyInterface::Ignore;
}

static bool hasMetaMethod(QObject* object, const char* methodName)
{
    int methodIndex = object->metaObject()->indexOfMethod(QMetaObject::normalizedSignature(methodName));
    return methodIndex >= 0 && methodIndex < object->metaObject()->methodCount();
}

/*!
    \qmlmethod NavigationPolicy DesktopWebView::navigationPolicyForUrl(url, button, modifiers)

    This method should be implemented by the user of DesktopWebView element.

    It will be called to decide the policy for a navigation: whether the WebView should ignore the navigation,
    continue it or start a download. The return value must be one of the policies in the NavigationPolicy enumeration.
*/
PolicyInterface::PolicyAction QDesktopWebViewPrivate::navigationPolicyForURL(const QUrl& url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    // We need to check this first because invokeMethod() warns if the method doesn't exist for the object.
    if (!hasMetaMethod(q, "navigationPolicyForUrl(QVariant,QVariant,QVariant)"))
        return PolicyInterface::Use;

    QVariant ret;
    if (QMetaObject::invokeMethod(q, "navigationPolicyForUrl", Q_RETURN_ARG(QVariant, ret), Q_ARG(QVariant, url), Q_ARG(QVariant, button), Q_ARG(QVariant, QVariant(modifiers))))
        return toPolicyAction(static_cast<QDesktopWebView::NavigationPolicy>(ret.toInt()));
    return PolicyInterface::Use;
}

#include "moc_qdesktopwebview.cpp"
