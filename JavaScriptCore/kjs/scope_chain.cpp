// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "scope_chain.h"

namespace KJS {

  struct ScopeChainNode {
    ScopeChainNode(const Value &val, ScopeChainNode *p, ScopeChainNode *n)
      : member(val.imp()), prev(p), next(n) { }
    ScopeChainNode(ValueImp *val, ScopeChainNode *p, ScopeChainNode *n)
      : member(val), prev(p), next(n) { }
    ValueImp *member;
    ScopeChainNode *prev, *next;
  };

  struct ScopeChainHookNode : public ScopeChainNode {
    ScopeChainHookNode(bool needsMarking) : ScopeChainNode(Value(), this, this),
        listRefCount(1), nodesRefCount(needsMarking ? 0 : 1) { }
    int listRefCount;
    int nodesRefCount;
  };

// ------------------------------ ScopeChainIterator ---------------------------------

ValueImp* ScopeChainIterator::operator->() const
{
  return node->member;
}

Value ScopeChainIterator::operator*() const
{
  return Value(node->member);
}

Value ScopeChainIterator::operator++()
{
  node = node->next;
  return Value(node->member);
}

Value ScopeChainIterator::operator++(int)
{
  const ScopeChainNode *n = node;
  ++*this;
  return Value(n->member);
}

// ------------------------------ ScopeChain -----------------------------------------

ScopeChain::ScopeChain(bool needsMarking) : hook(new ScopeChainHookNode(needsMarking)), m_needsMarking(needsMarking)
{
}

ScopeChain::ScopeChain(const ScopeChain& l) : hook(l.hook), m_needsMarking(false)
{
  ++hook->listRefCount;
  if (hook->nodesRefCount++ == 0)
    refAll();
}

ScopeChain& ScopeChain::operator=(const ScopeChain& l)
{
  ScopeChain(l).swap(*this);
  return *this;
}

ScopeChain::~ScopeChain()
{
  if (!m_needsMarking)
    if (--hook->nodesRefCount == 0)
      derefAll();
  
  if (--hook->listRefCount == 0) {
    assert(hook->nodesRefCount == 0);
    clearInternal();
    delete hook;
  }
}

void ScopeChain::mark() const
{
  ScopeChainNode *n = hook->next;
  while (n != hook) {
    if (!n->member->marked())
      n->member->mark();
    n = n->next;
  }
}

void ScopeChain::append(const Value& val)
{
  ScopeChainNode *n = new ScopeChainNode(val, hook->prev, hook);
  if (hook->nodesRefCount)
    n->member->ref();
  hook->prev->next = n;
  hook->prev = n;
}

void ScopeChain::prepend(const Value& val)
{
  ScopeChainNode *n = new ScopeChainNode(val, hook, hook->next);
  if (hook->nodesRefCount)
    n->member->ref();
  hook->next->prev = n;
  hook->next = n;
}

void ScopeChain::prepend(ValueImp *val)
{
  ScopeChainNode *n = new ScopeChainNode(val, hook, hook->next);
  if (hook->nodesRefCount)
    n->member->ref();
  hook->next->prev = n;
  hook->next = n;
}

void ScopeChain::prependList(const ScopeChain& lst)
{
  ScopeChainNode *otherHook = lst.hook;
  ScopeChainNode *n = otherHook->prev;
  while (n != otherHook) {
    prepend(n->member);
    n = n->prev;
  }
}

void ScopeChain::removeFirst()
{
  erase(hook->next);
}

void ScopeChain::clearInternal()
{
  ScopeChainNode *n = hook->next;
  while (n != hook) {
    n = n->next;
    delete n->prev;
  }

  hook->next = hook;
  hook->prev = hook;
}

ScopeChain ScopeChain::copy() const
{
  ScopeChain newScopeChain;
  newScopeChain.prependList(*this);
  return newScopeChain;
}

ScopeChainIterator ScopeChain::begin() const
{
  return ScopeChainIterator(hook->next);
}

ScopeChainIterator ScopeChain::end() const
{
  return ScopeChainIterator(hook);
}

void ScopeChain::erase(ScopeChainNode *n)
{
  if (n != hook) {
    if (hook->nodesRefCount)
      n->member->deref();
    n->next->prev = n->prev;
    n->prev->next = n->next;
    delete n;
  }
}

void ScopeChain::refAll() const
{
  for (ScopeChainNode *n = hook->next; n != hook; n = n->next)
    n->member->ref();
}

void ScopeChain::derefAll() const
{
  for (ScopeChainNode *n = hook->next; n != hook; n = n->next)
    n->member->deref();
}

void ScopeChain::swap(ScopeChain &other)
{
  if (!m_needsMarking)
    if (other.hook->nodesRefCount++ == 0)
      other.refAll();
  if (!other.m_needsMarking)
    if (hook->nodesRefCount++ == 0)
      refAll();

  if (!m_needsMarking)
    if (--hook->nodesRefCount == 0)
      derefAll();
  if (!other.m_needsMarking)
    if (--other.hook->nodesRefCount == 0)
      other.derefAll();

  ScopeChainHookNode *tmp = hook;
  hook = other.hook;
  other.hook = tmp;
}

bool ScopeChain::isEmpty() const
{
    return hook->next == hook;
}

} // namespace KJS
