/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQMapImpl.h"

KWQMapNodeImpl::KWQMapNodeImpl() :
    prev(NULL),
    next(NULL),
    prevIsChild(true),
    nextIsChild(true),
    color(Red)
{
}

KWQMapNodeImpl *KWQMapNodeImpl::left()
{
    return prevIsChild ? prev : NULL;
}

const KWQMapNodeImpl *KWQMapNodeImpl::left() const
{
    return prevIsChild ? prev : NULL;
}

KWQMapNodeImpl *KWQMapNodeImpl::right()
{
    return nextIsChild ? next : NULL;
}

const KWQMapNodeImpl *KWQMapNodeImpl::right() const
{
    return nextIsChild ? next : NULL;
}

KWQMapNodeImpl *KWQMapNodeImpl::predecessor()
{
    if (!prevIsChild || prev == NULL) {
	return prev;
    }
    KWQMapNodeImpl *pred = left();
    while (pred->right() != NULL) {
	pred = pred->right();
    }
    return pred;
}

const KWQMapNodeImpl *KWQMapNodeImpl::predecessor() const
{
    if (!prevIsChild || prev == NULL) {
	return prev;
    }
    const KWQMapNodeImpl *pred = left();
    while (pred->right() != NULL) {
	pred = pred->right();
    }
    return pred;
}

KWQMapNodeImpl *KWQMapNodeImpl::successor()
{
    if (!nextIsChild || next == NULL) {
	return next;
    }
    KWQMapNodeImpl *succ = right();
    while (succ->left() != NULL) {
	succ = succ->left();
    }
    return succ;
}

const KWQMapNodeImpl *KWQMapNodeImpl::successor() const
{
    if (!nextIsChild || next == NULL) {
	return next;
    }
    const KWQMapNodeImpl *succ = right();
    while (succ->left() != NULL) {
	succ = succ->left();
    }
    return succ;
}


KWQMapIteratorImpl::KWQMapIteratorImpl() : node(NULL)
{
}

void KWQMapIteratorImpl::incrementInternal()
{
    node = node->successor();
}


// KWQMapImplPrivate

class KWQMapImpl::KWQMapPrivate : public FastAllocated
{
public:
    KWQMapPrivate(KWQMapNodeImpl *node,
		  uint count,
		  void (*deleteFunc)(KWQMapNodeImpl *));

    ~KWQMapPrivate();

    KWQMapNodeImpl *guard;
    uint numNodes;
    int refCount;
    void (*deleteNode)(KWQMapNodeImpl *);
    friend class KWQRefPtr<KWQMapImpl::KWQMapPrivate>;
};

KWQMapImpl::KWQMapPrivate::KWQMapPrivate(KWQMapNodeImpl *node,
					 uint count,
					 void (*deleteFunc)(KWQMapNodeImpl *)) :
    guard(node),
    numNodes(count),
    refCount(0),
    deleteNode(deleteFunc)
{
}

KWQMapImpl::KWQMapPrivate::~KWQMapPrivate()
{
    deleteNode(guard);
}

// KWQMapImpl

KWQMapImpl::KWQMapImpl(KWQMapNodeImpl *guard, void (*deleteNode)(KWQMapNodeImpl *)) :
    d(new KWQMapPrivate(guard, 0, deleteNode))
{
}

KWQMapImpl::KWQMapImpl(const KWQMapImpl &impl) :
    d(impl.d)
{
}

KWQMapImpl::~KWQMapImpl()
{
}

void KWQMapImpl::copyOnWrite()
{
    if (d->refCount > 1) {
	d = KWQRefPtr<KWQMapPrivate>(new KWQMapPrivate(copyTree(d->guard, NULL, NULL), d->numNodes, d->deleteNode));
    }
}

KWQMapNodeImpl *KWQMapImpl::copyTree(const KWQMapNodeImpl *node, 
				     KWQMapNodeImpl *subtreePredecessor, 
				     KWQMapNodeImpl *subtreeSuccessor) const
{
    if (node == NULL) {
	return NULL;
    }
    
    // FIXME: not exception-safe - use auto_ptr?
    KWQMapNodeImpl *copy = duplicateNode(node);
    copy->color = node->color;
    
    if (node->prevIsChild) {
	copy->prevIsChild = true;
	copy->prev = copyTree(node->prev, subtreePredecessor, copy);
    } else {
	copy->prevIsChild = false;
	copy->prev = subtreePredecessor;
    }
    
    if (node->nextIsChild) {
	copy->nextIsChild = true;
	copy->next = copyTree(node->next, copy, subtreeSuccessor);
    } else {
	copy->nextIsChild = false;
	copy->next = subtreeSuccessor;
    }
    
    return copy;
}

void KWQMapImpl::rotateRight(KWQMapNodeImpl *node, KWQMapNodeImpl *parent, bool leftParent)
{
    KWQMapNodeImpl *rotationChild = node->left();

	if (leftParent) {
	    parent->prev = rotationChild;
	} else {
	    parent->next = rotationChild;
	}
	
	node->prev = rotationChild->next;
	node->prevIsChild = rotationChild->nextIsChild;
	
	rotationChild->next = node;
	rotationChild->nextIsChild = true;

	// fixup threads
	if (!node->prevIsChild) {
	    node->prev = rotationChild;
	}
    }
    
void KWQMapImpl::rotateLeft(KWQMapNodeImpl *node, KWQMapNodeImpl *parent, bool leftParent)
{
    KWQMapNodeImpl *rotationChild = node->right();
    
    if (leftParent) {
	parent->prev = rotationChild;
    } else {
	parent->next = rotationChild;
    }
    
    node->next = rotationChild->prev;
    node->nextIsChild = rotationChild->prevIsChild;
    
    rotationChild->prev = node;
    rotationChild->prevIsChild = true;
    
    // fixup threads
    if (!node->nextIsChild) {
	node->next = rotationChild;
    }
}

void KWQMapImpl::rebalanceAfterInsert(KWQMapNodeImpl **nodes, bool *wentLeft, int height)
{
    nodes[height]->color = KWQMapNodeImpl::Red;
    
    while (nodes[height] != d->guard->prev && nodes[height-1]->color == KWQMapNodeImpl::Red) {
	if (wentLeft[height-2]) {
	    KWQMapNodeImpl *uncle = nodes[height-2]->right();
	    if (uncle != NULL && uncle->color == KWQMapNodeImpl::Red) {
		nodes[height-1]->color = KWQMapNodeImpl::Black;
		uncle->color = KWQMapNodeImpl::Black;
		nodes[height-2]->color = KWQMapNodeImpl::Red;
		
		// go up two levels
		height = height - 2;
	    } else {
		KWQMapNodeImpl *parent;
		if (!wentLeft[height-1]) {
		    parent = nodes[height-1]->right();
		    rotateLeft(nodes[height-1], nodes[height-2], wentLeft[height-2]);
		} else {
		    parent = nodes[height-1];
		}
		parent->color = KWQMapNodeImpl::Black;
		nodes[height-2]->color = KWQMapNodeImpl::Red;
		rotateRight(nodes[height-2], nodes[height-3], wentLeft[height-3]);
		break;
	    }
	} else {
	    // same as the other branch, but with left and right swapped
	    
	    KWQMapNodeImpl *uncle = nodes[height-2]->left();
	    
	    if (uncle != NULL && uncle->color == KWQMapNodeImpl::Red) {
		nodes[height-1]->color = KWQMapNodeImpl::Black;
		uncle->color = KWQMapNodeImpl::Black;
		nodes[height-2]->color = KWQMapNodeImpl::Red;
		
		// go up two levels
		height = height - 2;
	    } else {
		KWQMapNodeImpl *parent;
		if (wentLeft[height-1]) {
		    parent = nodes[height-1]->left();
		    rotateRight(nodes[height-1], nodes[height-2], wentLeft[height-2]);
		} else {
		    parent = nodes[height-1];
		}
		parent->color = KWQMapNodeImpl::Black;
		nodes[height-2]->color = KWQMapNodeImpl::Red;
		rotateLeft(nodes[height-2], nodes[height-3], wentLeft[height-3]);
		break;
	    }
	}
    }
    
    d->guard->prev->color = KWQMapNodeImpl::Black;
}

void KWQMapImpl::rebalanceAfterRemove(KWQMapNodeImpl *nodeToRemove, KWQMapNodeImpl **nodes, bool *wentLeft, int height) 
{
    if (nodeToRemove->color == KWQMapNodeImpl::Black) {
	while (nodes[height] != d->guard->prev && (nodes[height]==NULL || nodes[height]->color==KWQMapNodeImpl::Black)) {
	    if (wentLeft[height-1]) {
		KWQMapNodeImpl *sibling = nodes[height-1]->right();
		if (sibling != NULL && sibling->color == KWQMapNodeImpl::Red) {
		    sibling->color = KWQMapNodeImpl::Black;
		    nodes[height-1]->color = KWQMapNodeImpl::Red;
		    rotateLeft(nodes[height-1], nodes[height-2], wentLeft[height-2]);
		    
		    nodes[height] = nodes[height-1];
		    wentLeft[height] = true;
		    nodes[height-1] = sibling;
		    height = height + 1;
		    
		    sibling = nodes[height-1]->right();
		}
		if ((sibling->left() == NULL || sibling->left()->color == KWQMapNodeImpl::Black) &&
		    (sibling->right() == NULL || sibling->right()->color == KWQMapNodeImpl::Black)) {
		    sibling->color = KWQMapNodeImpl::Red;
		    height = height - 1;
		} else {
		    if (sibling->right() == NULL || sibling->right()->color == KWQMapNodeImpl::Black) {
			sibling->left()->color = KWQMapNodeImpl::Black;
			sibling->color = KWQMapNodeImpl::Red;
			rotateRight(sibling, nodes[height-1], !wentLeft[height-1]);
			sibling = nodes[height-1]->right();
		    }
		    
		    sibling->color = nodes[height-1]->color;
		    nodes[height-1]->color = KWQMapNodeImpl::Black;
		    sibling->right()->color = KWQMapNodeImpl::Black;
		    rotateLeft(nodes[height-1], nodes[height-2], wentLeft[height-2]);
		    
		    nodes[height] = d->guard->prev;
		}
	    } else {
		// same as the other branch, but with left and right swapped
		
		KWQMapNodeImpl *sibling = nodes[height-1]->left();
		if (sibling != NULL && sibling->color == KWQMapNodeImpl::Red) {
		    sibling->color = KWQMapNodeImpl::Black;
		    nodes[height-1]->color = KWQMapNodeImpl::Red;
		    rotateRight(nodes[height-1], nodes[height-2], wentLeft[height-2]);
		    
		    nodes[height] = nodes[height-1];
		    wentLeft[height] = false;
		    nodes[height-1] = sibling;
		    height = height + 1;
		    
		    sibling = nodes[height-1]->left();
		}
		if ((sibling->right() == NULL || sibling->right()->color == KWQMapNodeImpl::Black) &&
		    (sibling->left() == NULL || sibling->left()->color == KWQMapNodeImpl::Black)) {
		    sibling->color = KWQMapNodeImpl::Red;
		    height = height - 1;
		} else {
		    if (sibling->left() == NULL || sibling->left()->color == KWQMapNodeImpl::Black) {
			sibling->right()->color = KWQMapNodeImpl::Black;
			sibling->color = KWQMapNodeImpl::Red;
			rotateLeft(sibling, nodes[height-1], !wentLeft[height-1]);
			sibling = nodes[height-1]->left();
		    }
		    
		    sibling->color = nodes[height-1]->color;
		    nodes[height-1]->color = KWQMapNodeImpl::Black;
		    sibling->left()->color = KWQMapNodeImpl::Black;
		    rotateRight(nodes[height-1], nodes[height-2], wentLeft[height-2]);
		    
		    nodes[height] = d->guard->prev;
		}
	    }
	}
	
	if (nodes[height] != NULL) {
	    nodes[height]->color = KWQMapNodeImpl::Black;
	}
    }
}

KWQMapNodeImpl *KWQMapImpl::findInternal(KWQMapNodeImpl *target) const
{
    KWQMapNodeImpl *node = d->guard->left();
    
    while (node != NULL) {
	CompareResult compare = compareNodes(target,node);
	
	if (compare == Equal) {
	    break;
	} else if (compare == Less) {
	    node = node->left();
	} else if (compare == Greater) {
	    node = node->right();
	}
    }
    
    return node;
}

KWQMapNodeImpl *KWQMapImpl::insertInternal(KWQMapNodeImpl *nodeToInsert, bool replaceExisting)
{
    KWQMapNodeImpl *nodeStack[MAX_STACK];
    bool wentLeftStack[MAX_STACK];
    int height = 0;
    
    copyOnWrite();

    nodeStack[height] = d->guard;
    wentLeftStack[height] = true;
    height++;
    
    KWQMapNodeImpl *node = d->guard->left();
    
    while (node != NULL) {
	CompareResult compare = compareNodes(nodeToInsert, node);
	if (compare == Equal) {
	    break;
	} else if (compare == Less) {
	    nodeStack[height] = node;
	    wentLeftStack[height] = true;
	    height++;
	    node = node->left();
	} else {
	    nodeStack[height] = node;
	    wentLeftStack[height] = false;
	    height++;
	    node = node->right();
	}
    }
    
    if (node != NULL) {
	if (replaceExisting) {
	    copyNode(nodeToInsert, node);
	}
	return node;
    }
    
    node = duplicateNode(nodeToInsert);
    
    nodeStack[height] = node;
    
    if (wentLeftStack[height-1]) {
	// arrange for threading
	node->prev = nodeStack[height-1]->prev;
	node->prevIsChild = false;
	node->next = nodeStack[height-1];
	node->nextIsChild = false;
	
	// make it the child
	nodeStack[height-1]->prev = node;
	nodeStack[height-1]->prevIsChild = true;
    } else {
	// arrange for threading
	node->next = nodeStack[height-1]->next;
	node->nextIsChild = false;
	node->prev = nodeStack[height-1];
	node->prevIsChild = false;
	
	// make it the child
	nodeStack[height-1]->next = node;
	nodeStack[height-1]->nextIsChild = true;
    }
    
    rebalanceAfterInsert(nodeStack, wentLeftStack, height);
    d->numNodes++;

    return node;
}

void KWQMapImpl::removeEqualInternal(KWQMapNodeImpl *nodeToDelete, bool samePointer)
{
    KWQMapNodeImpl *nodeStack[MAX_STACK];
    bool wentLeftStack[MAX_STACK];
    int height = 0;
    
    copyOnWrite();

    nodeStack[height] = d->guard;
    wentLeftStack[height] = true;
    height++;
    
    KWQMapNodeImpl *node = d->guard->left();
    
    while (node != NULL) {
	CompareResult compare = compareNodes(nodeToDelete, node);
	if (compare == Equal) {
	    break;
	} else if (compare == Less) {
	    nodeStack[height] = node;
	    wentLeftStack[height] = true;
	    height++;
	    node = node->left();
	} else {
	    nodeStack[height] = node;
	    wentLeftStack[height] = false;
	    height++;
	    node = node->right();
	}
    }

    if (node == NULL || samePointer && node != nodeToDelete) {
	return;
    }
	
    KWQMapNodeImpl *removalParent;
    KWQMapNodeImpl *nodeToRemove;
    bool removalWentLeft;

    if (node->left() == NULL || node->right() == NULL) {
	// If this node has at most one subtree, we can remove it directly
	nodeToRemove = node;
	removalParent = nodeStack[height-1];
	removalWentLeft = wentLeftStack[height-1];
    } else {
	// Otherwise we find the immediate successor (leftmost
	// node in the right subtree), which must have at most one
	// subtree, swap it's contents with the node we found, and
	// remove it.
	nodeToRemove = node->right();
	wentLeftStack[height] = false;
	nodeStack[height] = node;
	height++;
	
	removalParent = node;
	removalWentLeft = false;
	
	while (nodeToRemove->left() != NULL) {
	    wentLeftStack[height] = true;
	    nodeStack[height] = nodeToRemove;
	    removalParent = nodeToRemove;
	    nodeToRemove = nodeToRemove->left();
	    removalWentLeft = true;
	    height++;
	}

	swapNodes(node,nodeToRemove);
    }


    // find replacement node
    KWQMapNodeImpl *removalReplacement;

    if (nodeToRemove->left() != NULL) {
	removalReplacement = nodeToRemove->left();
	// fixup threading
	nodeToRemove->predecessor()->next = nodeToRemove->next; 
    } else if (nodeToRemove->right() != NULL) {
	removalReplacement = nodeToRemove->right();
	// fixup threading
	nodeToRemove->successor()->prev = nodeToRemove->prev;
    } else {
	removalReplacement = NULL;
    }
    
    nodeStack[height] = removalReplacement;
    
    // detach removal node
    if (removalWentLeft) {
	if (removalReplacement == NULL) {
	    removalParent->prev = nodeToRemove->prev;
	    removalParent->prevIsChild = nodeToRemove->prevIsChild;
	} else {
	    removalParent->prev = removalReplacement;
	}
    } else {
	if (removalReplacement == NULL) {
	    removalParent->next = nodeToRemove->next;
	    removalParent->nextIsChild = nodeToRemove->nextIsChild;
	} else {
	    removalParent->next = removalReplacement;
	}
    }
    
    // do red-black rebalancing
    rebalanceAfterRemove(nodeToRemove, nodeStack, wentLeftStack, height);
    
    // detach removal node's children
    nodeToRemove->next = NULL;
    nodeToRemove->prev = NULL;
    
    d->numNodes--;
    d->deleteNode(nodeToRemove);
}

void KWQMapImpl::swap(KWQMapImpl &map)
{
    KWQRefPtr<KWQMapPrivate> tmp = d;
    d = map.d;
    map.d = d;
}

uint KWQMapImpl::countInternal() const
{
    return d->numNodes;
}

void KWQMapImpl::clearInternal()
{
    copyOnWrite();

    d->deleteNode(d->guard->prev);
    d->guard->prev = NULL;
    d->numNodes = 0;
}

const KWQMapNodeImpl *KWQMapImpl::beginInternal() const
{
    KWQMapNodeImpl *node;
    
    node = d->guard;
    while (node->left() != NULL) {
	node = node->left();
    }
    
    return node;
}

const KWQMapNodeImpl *KWQMapImpl::endInternal() const
{
    return d->guard;
}

KWQMapNodeImpl *KWQMapImpl::beginInternal()
{
    KWQMapNodeImpl *node;
    
    copyOnWrite();

    node = d->guard;
    while (node->left() != NULL) {
	node = node->left();
    }
    
    return node;
}

KWQMapNodeImpl *KWQMapImpl::endInternal()
{
    copyOnWrite();
    return d->guard;
}
