/*
 * Copyright (C) 2011, Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GtkDragAndDropHelper.h"

#if ENABLE(DRAG_SUPPORT)

#include "ClipboardUtilitiesGtk.h"
#include "DragData.h"
#include "GtkUtilities.h"
#include "GtkVersioning.h"
#include "PasteboardHelper.h"
#include <gtk/gtk.h>

namespace WebCore {

struct DroppingContext {
    DroppingContext(GdkDragContext* gdkContext, const IntPoint& position)
        : gdkContext(gdkContext)
        , dataObject(DataObjectGtk::create())
        , lastMotionPosition(position)
        , dropHappened(false)
        , exitedCallback(0)
    {
    }

    GdkDragContext* gdkContext;
    RefPtr<DataObjectGtk> dataObject;
    WebCore::IntPoint lastMotionPosition;
    int pendingDataRequests;
    bool dropHappened;
    DragExitedCallback exitedCallback;
};

GtkDragAndDropHelper::GtkDragAndDropHelper()
    : m_widget(nullptr)
{
}

GtkDragAndDropHelper::~GtkDragAndDropHelper()
{
}

bool GtkDragAndDropHelper::handleDragEnd(GdkDragContext* dragContext)
{
    return m_draggingDataObjects.remove(dragContext);
}

void GtkDragAndDropHelper::handleGetDragData(GdkDragContext* context, GtkSelectionData* selectionData, guint info)
{
    DataObjectGtk* dataObject = m_draggingDataObjects.get(context);
    if (!dataObject)
        return;
    PasteboardHelper::defaultPasteboardHelper()->fillSelectionData(selectionData, info, dataObject);
}

struct HandleDragLaterData {
    DroppingContext* context;
    GtkDragAndDropHelper* glue;
};

static gboolean handleDragLeaveLaterCallback(HandleDragLaterData* data)
{
    data->glue->handleDragLeaveLater(data->context);
    delete data;
    return FALSE;
}

void GtkDragAndDropHelper::handleDragLeaveLater(DroppingContext* context)
{
    auto iterator = m_droppingContexts.find(context->gdkContext);
    if (iterator == m_droppingContexts.end())
        return;

    // If the view doesn't know about the drag yet (there are still pending data)
    // requests, don't update it with information about the drag.
    if (context->pendingDataRequests)
        return;

    const IntPoint& position = context->lastMotionPosition;
    DragData dragData(context->dataObject.get(), position,
                      convertWidgetPointToScreenPoint(m_widget, position),
                      DragOperationNone);
    context->exitedCallback(m_widget, dragData, context->dropHappened);

    m_droppingContexts.remove(iterator);
}

void GtkDragAndDropHelper::handleDragLeave(GdkDragContext* gdkContext, DragExitedCallback exitedCallback)
{
    DroppingContext* context = m_droppingContexts.get(gdkContext);
    if (!context)
        return;

    // During a drop GTK+ will fire a drag-leave signal right before firing
    // the drag-drop signal. We want the actions for drag-leave to happen after
    // those for drag-drop, so schedule them to happen asynchronously here.
    HandleDragLaterData* data = new HandleDragLaterData;
    data->context = context;
    data->context->exitedCallback = exitedCallback;
    data->glue = this;
    g_idle_add_full(G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(handleDragLeaveLaterCallback), data, 0);
}

static void queryNewDropContextData(DroppingContext* dropContext, GtkWidget* widget, guint time)
{
    GdkDragContext* gdkContext = dropContext->gdkContext;
    Vector<GdkAtom> acceptableTargets(PasteboardHelper::defaultPasteboardHelper()->dropAtomsForContext(widget, gdkContext));
    dropContext->pendingDataRequests = acceptableTargets.size();
    for (size_t i = 0; i < acceptableTargets.size(); i++)
        gtk_drag_get_data(widget, gdkContext, acceptableTargets.at(i), time);
}

DataObjectGtk* GtkDragAndDropHelper::handleDragMotion(GdkDragContext* context, const IntPoint& position, unsigned time)
{
    std::unique_ptr<DroppingContext>& droppingContext = m_droppingContexts.add(context, nullptr).iterator->value;
    if (!droppingContext) {
        droppingContext = std::make_unique<DroppingContext>(context, position);
        queryNewDropContextData(droppingContext.get(), m_widget, time);
    } else
        droppingContext->lastMotionPosition = position;

    // Don't send any drag information to WebCore until we've retrieved all
    // the data for this drag operation. Otherwise we'd have to block to wait
    // for the drag's data.
    if (droppingContext->pendingDataRequests > 0)
        return nullptr;

    return droppingContext->dataObject.get();
}

DataObjectGtk* GtkDragAndDropHelper::handleDragDataReceived(GdkDragContext* context, GtkSelectionData* selectionData, unsigned info, IntPoint& position)
{
    DroppingContext* droppingContext = m_droppingContexts.get(context);
    if (!droppingContext)
        return nullptr;

    droppingContext->pendingDataRequests--;
    PasteboardHelper::defaultPasteboardHelper()->fillDataObjectFromDropData(selectionData, info, droppingContext->dataObject.get());

    if (droppingContext->pendingDataRequests)
        return nullptr;

    // The coordinates passed to drag-data-received signal are sometimes
    // inaccurate in DRT, so use the coordinates of the last motion event.
    position = droppingContext->lastMotionPosition;

    // If there are no more pending requests, start sending dragging data to WebCore.
    return droppingContext->dataObject.get();
}

DataObjectGtk* GtkDragAndDropHelper::handleDragDrop(GdkDragContext* context)
{
    DroppingContext* droppingContext = m_droppingContexts.get(context);
    if (!droppingContext)
        return nullptr;

    droppingContext->dropHappened = true;

    return droppingContext->dataObject.get();
}

void GtkDragAndDropHelper::startedDrag(GdkDragContext* context, DataObjectGtk* dataObject)
{
    m_draggingDataObjects.set(context, dataObject);
}

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
