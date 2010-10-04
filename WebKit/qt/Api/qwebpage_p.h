/*
    Copyright (C) 2008, 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef QWEBPAGE_P_H
#define QWEBPAGE_P_H

#include <qbasictimer.h>
#include <qnetworkproxy.h>
#include <qpointer.h>
#include <qevent.h>
#include <qgraphicssceneevent.h>

#include "qwebpage.h"
#include "qwebhistory.h"
#include "qwebframe.h"

#include "KURL.h"
#include "PlatformString.h"

#include <wtf/RefPtr.h>

#include "ViewportArguments.h"

namespace WebCore {
    class ChromeClientQt;
    class ContextMenuClientQt;
    class ContextMenuItem;
    class ContextMenu;
    class EditorClientQt;
    class Element;
    class InspectorController;
    class Node;
    class Page;
    class Frame;
}

QT_BEGIN_NAMESPACE
class QUndoStack;
class QMenu;
class QBitArray;
QT_END_NAMESPACE

class QWebInspector;
class QWebPageClient;

class QtViewportConfigurationPrivate : public QSharedData {
public:
    QtViewportConfigurationPrivate(QWebPage::ViewportConfiguration* qq)
        : q(qq)
    { }

    QWebPage::ViewportConfiguration* q;
};

class QWebPagePrivate {
public:
    QWebPagePrivate(QWebPage*);
    ~QWebPagePrivate();

    static WebCore::Page* core(const QWebPage*);
    static QWebPagePrivate* priv(QWebPage*);

    void createMainFrame();
#ifndef QT_NO_CONTEXTMENU
    QMenu* createContextMenu(const WebCore::ContextMenu* webcoreMenu, const QList<WebCore::ContextMenuItem>* items, QBitArray* visitedWebActions);
#endif
    void _q_onLoadProgressChanged(int);
    void _q_webActionTriggered(bool checked);
    void _q_cleanupLeakMessages();
    void updateAction(QWebPage::WebAction action);
    void updateNavigationActions();
    void updateEditorActions();

    void timerEvent(QTimerEvent*);

    void mouseMoveEvent(QMouseEvent*);
    void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    void mousePressEvent(QMouseEvent*);
    void mousePressEvent(QGraphicsSceneMouseEvent*);
    void mouseDoubleClickEvent(QMouseEvent*);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);
    void mouseTripleClickEvent(QMouseEvent*);
    void mouseTripleClickEvent(QGraphicsSceneMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(const QPoint& globalPos);
#endif
#ifndef QT_NO_WHEELEVENT
    void wheelEvent(QWheelEvent*);
    void wheelEvent(QGraphicsSceneWheelEvent*);
#endif
    void keyPressEvent(QKeyEvent*);
    void keyReleaseEvent(QKeyEvent*);
    void focusInEvent(QFocusEvent*);
    void focusOutEvent(QFocusEvent*);

    void dragEnterEvent(QDragEnterEvent*);
    void dragEnterEvent(QGraphicsSceneDragDropEvent*);
    void dragLeaveEvent(QDragLeaveEvent*);
    void dragLeaveEvent(QGraphicsSceneDragDropEvent*);
    void dragMoveEvent(QDragMoveEvent*);
    void dragMoveEvent(QGraphicsSceneDragDropEvent*);
    void dropEvent(QDropEvent*);
    void dropEvent(QGraphicsSceneDragDropEvent*);

    void inputMethodEvent(QInputMethodEvent*);

#ifndef QT_NO_PROPERTIES
    void dynamicPropertyChangeEvent(QDynamicPropertyChangeEvent*);
#endif

    void shortcutOverrideEvent(QKeyEvent*);
    void leaveEvent(QEvent*);
    void handleClipboard(QEvent*, Qt::MouseButton);
    void handleSoftwareInputPanel(Qt::MouseButton);
    bool handleScrolling(QKeyEvent*, WebCore::Frame*);

    // Returns whether the default action was cancelled in the JS event handler
    bool touchEvent(QTouchEvent*);

    void setInspector(QWebInspector*);
    QWebInspector* getOrCreateInspector();
    WebCore::InspectorController* inspectorController();
    quint16 inspectorServerPort();

    WebCore::ViewportArguments viewportArguments();

#ifndef QT_NO_SHORTCUT
    static QWebPage::WebAction editorActionForKeyEvent(QKeyEvent* event);
#endif
    static const char* editorCommandForWebActions(QWebPage::WebAction action);

    QWebPage *q;
    WebCore::Page *page;
    QWebPageClient* client;
    QPointer<QWebFrame> mainFrame;

#ifndef QT_NO_UNDOSTACK
    QUndoStack *undoStack;
#endif

    QWeakPointer<QWidget> view;

    bool insideOpenCall;
    quint64 m_totalBytes;
    quint64 m_bytesReceived;

    QPoint tripleClick;
    QBasicTimer tripleClickTimer;

    bool clickCausedFocus;

    bool acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, QWebPage::NavigationType type);
    QNetworkAccessManager *networkManager;

    bool forwardUnsupportedContent;
    bool smartInsertDeleteEnabled;
    bool selectTrailingWhitespaceEnabled;
    QWebPage::LinkDelegationPolicy linkPolicy;

    QSize viewportSize;
    QSize fixedLayoutSize;
    QWebHistory history;
    QWebHitTestResult hitTestResult;
#ifndef QT_NO_CONTEXTMENU
    QPointer<QMenu> currentContextMenu;
#endif
    QWebSettings *settings;
    QPalette palette;
    bool editable;
    bool useFixedLayout;

    QAction *actions[QWebPage::WebActionCount];

    QWebPluginFactory *pluginFactory;

    QWidget* inspectorFrontend;
    QWebInspector* inspector;
    bool inspectorIsInternalOnly; // True if created through the Inspect context menu action
    Qt::DropAction m_lastDropAction;

    static bool drtRun;
};

#endif
