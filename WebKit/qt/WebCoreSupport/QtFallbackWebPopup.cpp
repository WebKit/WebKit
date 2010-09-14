/*
 * Copyright (C) 2010 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#include "config.h"
#include "QtFallbackWebPopup.h"

#ifndef QT_NO_COMBOBOX

#include "ChromeClientQt.h"
#include "QWebPageClient.h"
#include "qgraphicswebview.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QInputContext>
#include <QMouseEvent>
#include <QStandardItemModel>

#if ENABLE(SYMBIAN_DIALOG_PROVIDERS)
#include <BrCtlDialogsProvider.h>
#include <BrowserDialogsProvider.h> // S60 platform private header file
#include <e32base.h>
#endif

namespace WebCore {

QtFallbackWebPopupCombo::QtFallbackWebPopupCombo(QtFallbackWebPopup& ownerPopup)
    : m_ownerPopup(ownerPopup)
{
    // Install an event filter on the view inside the combo box popup to make sure we know
    // when the popup got closed. E.g. QComboBox::hidePopup() won't be called when the popup
    // is closed by a mouse wheel event outside its window.
    view()->installEventFilter(this);
}

void QtFallbackWebPopupCombo::showPopup()
{
    QComboBox::showPopup();
    m_ownerPopup.m_popupVisible = true;
}

void QtFallbackWebPopupCombo::hidePopup()
{
#ifndef QT_NO_IM
    QWidget* activeFocus = QApplication::focusWidget();
    if (activeFocus && activeFocus == QComboBox::view()
        && activeFocus->testAttribute(Qt::WA_InputMethodEnabled)) {
        QInputContext* qic = activeFocus->inputContext();
        if (qic) {
            qic->reset();
            qic->setFocusWidget(0);
        }
    }
#endif // QT_NO_IM

    QComboBox::hidePopup();

    if (!m_ownerPopup.m_popupVisible)
        return;

    m_ownerPopup.m_popupVisible = false;
    emit m_ownerPopup.didHide();
    m_ownerPopup.destroyPopup();
}

bool QtFallbackWebPopupCombo::eventFilter(QObject* watched, QEvent* event)
{
    Q_ASSERT(watched == view());

    if (event->type() == QEvent::Show && !m_ownerPopup.m_popupVisible)
        showPopup();
    else if (event->type() == QEvent::Hide && m_ownerPopup.m_popupVisible)
        hidePopup();

    return false;
}

// QtFallbackWebPopup

QtFallbackWebPopup::QtFallbackWebPopup(const ChromeClientQt* chromeClient)
    : m_popupVisible(false)
    , m_combo(0)
    , m_chromeClient(chromeClient)
{
}

QtFallbackWebPopup::~QtFallbackWebPopup()
{
    destroyPopup();
}

void QtFallbackWebPopup::show(const QWebSelectData& data)
{
    if (!pageClient())
        return;

#if ENABLE(SYMBIAN_DIALOG_PROVIDERS)
    TRAP_IGNORE(showS60BrowserDialog());
#else

    destroyPopup();
    m_combo = new QtFallbackWebPopupCombo(*this);
    connect(m_combo, SIGNAL(activated(int)),
            SLOT(activeChanged(int)), Qt::QueuedConnection);

    populate(data);

    QRect rect = geometry();
    if (QGraphicsWebView *webView = qobject_cast<QGraphicsWebView*>(pageClient()->pluginParent())) {
        QGraphicsProxyWidget* proxy = new QGraphicsProxyWidget(webView);
        proxy->setWidget(m_combo);
        proxy->setGeometry(rect);
    } else {
        m_combo->setParent(pageClient()->ownerWidget());
        m_combo->setGeometry(QRect(rect.left(), rect.top(),
                               rect.width(), m_combo->sizeHint().height()));

    }

    QMouseEvent event(QEvent::MouseButtonPress, QCursor::pos(), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m_combo, &event);
#endif
}

#if ENABLE(SYMBIAN_DIALOG_PROVIDERS)

static void ResetAndDestroy(TAny* aPtr)
{
    RPointerArray<HBufC>* items = reinterpret_cast<RPointerArray<HBufC>* >(aPtr);
    items->ResetAndDestroy();
}

void QtFallbackWebPopup::showS60BrowserDialog()
{
    static MBrCtlDialogsProvider* dialogs = CBrowserDialogsProvider::NewL(0);
    if (!dialogs)
        return;

    int size = itemCount();
    CArrayFix<TBrCtlSelectOptionData>* options = new CArrayFixFlat<TBrCtlSelectOptionData>(qMax(1, size));
    RPointerArray<HBufC> items(qMax(1, size));
    CleanupStack::PushL(TCleanupItem(&ResetAndDestroy, &items));

    for (int i = 0; i < size; i++) {
        if (itemType(i) == Separator) {
            TBrCtlSelectOptionData data(_L("----------"), false, false, false);
            options->AppendL(data);
        } else {
            HBufC16* itemStr = HBufC16::NewL(itemText(i).length());
            itemStr->Des().Copy((const TUint16*)itemText(i).utf16(), itemText(i).length());
            CleanupStack::PushL(itemStr);
            TBrCtlSelectOptionData data(*itemStr, i == currentIndex(), false, itemIsEnabled(i));
            options->AppendL(data);
            items.AppendL(itemStr);
            CleanupStack::Pop();
        }
    }

    dialogs->DialogSelectOptionL(KNullDesC(), (TBrCtlSelectOptionType)(ESelectTypeSingle | ESelectTypeWithFindPane), *options);

    CleanupStack::PopAndDestroy(&items);

    int newIndex;
    for (newIndex = 0; newIndex < options->Count() && !options->At(newIndex).IsSelected(); newIndex++) {}
    if (newIndex == options->Count())
        newIndex = currentIndex();
    
    m_popupVisible = false;
    popupDidHide();

    if (currentIndex() != newIndex && newIndex >= 0)
        valueChanged(newIndex);

    delete options;
}
#endif

void QtFallbackWebPopup::hide()
{
    // Destroying the QComboBox here cause problems if the popup is in the
    // middle of its show animation. Instead we rely on the fact that the
    // Qt::Popup window will hide itself on mouse events outside its window.
}

void QtFallbackWebPopup::destroyPopup()
{
    if (m_combo) {
        m_combo->deleteLater();
        m_combo = 0;
    }
}

void QtFallbackWebPopup::populate(const QWebSelectData& data)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_combo->model());
    Q_ASSERT(model);

#if !defined(Q_OS_SYMBIAN)
    m_combo->setFont(font());
#endif

    int currentIndex = -1;
    for (int i = 0; i < data.itemCount(); ++i) {
        switch (data.itemType(i)) {
        case QWebSelectData::Separator:
            m_combo->insertSeparator(i);
            break;
        case QWebSelectData::Group:
            m_combo->insertItem(i, data.itemText(i));
            model->item(i)->setEnabled(false);
            break;
        case QWebSelectData::Option:
            m_combo->insertItem(i, data.itemText(i));
            model->item(i)->setEnabled(data.itemIsEnabled(i));
            model->item(i)->setToolTip(data.itemToolTip(i));
            if (data.itemIsSelected(i))
                currentIndex = i;
            break;
        }
    }

    if (currentIndex >= 0)
        m_combo->setCurrentIndex(currentIndex);
}

void QtFallbackWebPopup::activeChanged(int index)
{
    if (index < 0)
        return;

    emit selectItem(index, false, false);
}

QWebPageClient* QtFallbackWebPopup::pageClient() const
{
    return m_chromeClient->platformPageClient();
}

}

#endif // QT_NO_COMBOBOX
