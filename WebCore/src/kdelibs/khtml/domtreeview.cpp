/***************************************************************************
                               domtreeview.cpp
                             -------------------

    copyright            : (C) 2001 - The Kafka Team
    email                : kde-kafka@master.kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "khtml_part.h"
#include "domtreeview.moc"

DOMTreeView::DOMTreeView(QWidget *parent, KHTMLPart *currentpart, const char * name) : KListView(parent, name)
{
    setCaption(name);
    setRootIsDecorated(true);
    addColumn("Name");
    addColumn("Value");
    setSorting(-1);
    part = currentpart;
    connect(part, SIGNAL(nodeActivated(const DOM::Node &)), this, SLOT(showTree(const DOM::Node &)));
    connect(this, SIGNAL(clicked(QListViewItem *)), this, SLOT(slotItemClicked(QListViewItem *)));
    m_nodedict.setAutoDelete(true);
}

DOMTreeView::~DOMTreeView()
{
    disconnect(part);
}

void DOMTreeView::showTree(const DOM::Node &pNode)
{
    if(pNode.isNull() || document != pNode.ownerDocument())
    {
	clear();
	m_itemdict.clear();
	m_nodedict.clear();
	if(pNode.isNull())
	    return;
	else if(pNode.ownerDocument().isNull())
	    recursive(0, pNode);
	else
	    recursive(0, pNode.ownerDocument());
    }
    setCurrentItem(m_itemdict[pNode.handle()]);
    ensureItemVisible(m_itemdict[pNode.handle()]);
}

void DOMTreeView::recursive(const DOM::Node &pNode, const DOM::Node &node)
{
    QListViewItem *cur_item;
    if(pNode.ownerDocument() != document)
    {
	cur_item = new QListViewItem(static_cast<QListView *>(this), node.nodeName().string(), node.nodeValue().string());
	document = pNode.ownerDocument();
    }
    else
	cur_item = new QListViewItem(m_itemdict[pNode.handle()], node.nodeName().string(), node.nodeValue().string());

    if(node.handle())
    {
	m_itemdict.insert(node.handle(), cur_item);
	m_nodedict.insert(cur_item, new DOM::Node(node));
    }

    DOM::Node cur_child = node.lastChild();
    while(!cur_child.isNull())
    {
	recursive(node, cur_child);
	cur_child = cur_child.previousSibling();
    }
}

void DOMTreeView::slotItemClicked(QListViewItem *cur_item)
{
    DOM::Node *handle = m_nodedict[cur_item];
    if(handle)
	emit part->setActiveNode(*handle);
}
