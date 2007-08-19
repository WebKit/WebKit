/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
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

#include "config.h"
#include "ContextMenu.h"
#include "MenuEventProxy.h"

#include <wtf/Assertions.h>

#include <QAction>

#include <Document.h>
#include <Frame.h>
#include <FrameView.h>

namespace WebCore {

ContextMenu::ContextMenu(const HitTestResult& result)
    : m_hitTestResult(result)
{
#ifndef QT_NO_MENU
    m_menu = new QMenu;
    //qDebug("Create menu(%p) %p", this, (QMenu*)m_menu);
    m_proxy = new MenuEventProxy(this);
    QObject::connect(m_menu, SIGNAL(triggered(QAction *)), m_proxy, SLOT(trigger(QAction *)));
#endif
}

ContextMenu::~ContextMenu()
{
#ifndef QT_NO_MENU
    //qDebug("Destroy menu(%p) %p", this, (QMenu*)m_menu);
    delete m_menu;
    m_menu = 0;
    delete m_proxy;
    m_proxy = 0;
#endif
}

void ContextMenu::appendItem(ContextMenuItem& item)
{
    insertItem(999999, item); // yuck!  Fix this API!!
}

unsigned ContextMenu::itemCount() const
{
#ifndef QT_NO_MENU
    return m_menu->actions().count();
#else
    return 0;
#endif
}

void ContextMenu::insertItem(unsigned position, ContextMenuItem& item)
{
#ifndef QT_NO_MENU
    int id;
    QAction *action;
    QAction *before = 0;
    int p = position;
    if (p == 999999)
        p = -1;
    if (p >= 0)
        before = m_menu->actions()[p];

    switch (item.type()) {
        case ActionType:
            if (!item.title().isEmpty()) {
                action = m_menu->addAction((QString)item.title());
                m_menu->removeAction(action);
                m_menu->insertAction(before, action);
            }
            break;
        case SeparatorType:
            action = m_menu->insertSeparator(before);
            break;
        case SubmenuType:
            if (!item.title().isEmpty()) {
                QMenu *m = item.platformSubMenu();
                if (!m)
                    return;
                action = m_menu->insertMenu(before, m);
                action->setText(item.title());
            }
            break;
        default:
            return;
    }
    if (action) {
        m_proxy->map(action, item.action());
        item.releasePlatformDescription()->qaction = action;
    }
#endif
}

void ContextMenu::setPlatformDescription(PlatformMenuDescription menu)
{
#ifndef QT_NO_MENU
    //qDebug("Switch menu(%p) %p to %p", this, (QMenu*)m_menu, menu);
    if (menu == 0) {
        FrameView *v = m_hitTestResult.innerNode()->document()->frame()->view();
        m_menu->exec(v->containingWindow()->mapToGlobal(v->contentsToWindow(m_hitTestResult.point())));
    }
    if (menu != m_menu) {
        delete m_menu;
        m_menu = menu;
    }
#endif
}

PlatformMenuDescription ContextMenu::platformDescription() const
{
    return m_menu;
}

PlatformMenuDescription ContextMenu::releasePlatformDescription()
{
    QMenu* tmp = m_menu;
    m_menu = 0;
    return tmp;
}


}
// vim: ts=4 sw=4 et
