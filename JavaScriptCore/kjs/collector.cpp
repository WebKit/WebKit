/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "collector.h"
#include "object.h"
#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

namespace KJS {

  class CollectorBlock {
  public:
    CollectorBlock(int s);
    ~CollectorBlock();
    int size;
    int filled;
    void** mem;
    CollectorBlock *prev, *next;
  };

}; // namespace

using namespace KJS;

CollectorBlock::CollectorBlock(int s)
  : size(s),
    filled(0),
    prev(0L),
    next(0L)
{
  mem = new void*[size];
  memset(mem, 0, size * sizeof(void*));
}

CollectorBlock::~CollectorBlock()
{
  delete [] mem;
  mem = 0L;
}

CollectorBlock* Collector::root = 0L;
CollectorBlock* Collector::currentBlock = 0L;
unsigned long Collector::filled = 0;
unsigned long Collector::softLimit = KJS_MEM_INCREMENT;
#ifdef KJS_DEBUG_MEM
bool Collector::collecting = false;
#endif

void* Collector::allocate(size_t s)
{
  if (s == 0)
    return 0L;

  if (filled >= softLimit) {
    collect();
    if (filled >= softLimit && softLimit < KJS_MEM_LIMIT) // we are actually using all this memory
      softLimit *= 2;
  }

  void *m = malloc(s);

  // hack to ensure obj is protected from GC before any constructors are run
  // (prev = marked, next = gcallowed)
  static_cast<Imp*>(m)->prev = 0;
  static_cast<Imp*>(m)->next = 0;

  if (!root) {
    root = new CollectorBlock(BlockSize);
    currentBlock = root;
  }

  CollectorBlock *block = currentBlock;
  if (!block)
    block = root;

  // search for a block with space left
  while (block->next && block->filled == block->size)
    block = block->next;

  if (block->filled >= block->size) {
#ifdef KJS_DEBUG_MEM
    printf("allocating new block of size %d\n", block->size);
#endif
    CollectorBlock *tmp = new CollectorBlock(BlockSize);
    block->next = tmp;
    tmp->prev = block;
    block = tmp;
  }
  currentBlock = block;
  // look for a free spot in the block
  void **r = block->mem;
  while (*r)
    r++;
  *r = m;
  filled++;
  block->filled++;

  if (softLimit >= KJS_MEM_LIMIT) {
      KJScriptImp::setException("Out of memory");
  }

  return m;
}

/**
 * Mark-sweep garbage collection.
 */
void Collector::collect()
{
#ifdef KJS_DEBUG_MEM
  printf("collecting %d objects total\n", Imp::count);
  collecting = true;
#endif

  // MARK: first set all ref counts to 0 ....
  CollectorBlock *block = root;
  while (block) {
#ifdef KJS_DEBUG_MEM
    printf("cleaning block filled %d out of %d\n", block->filled, block->size);
#endif
    Imp **r = (Imp**)block->mem;
    assert(r);
    for (int i = 0; i < block->size; i++, r++)
      if (*r) {
        (*r)->setMarked(false);
      }
    block = block->next;
  }

  // ... increase counter for all referenced objects recursively
  // starting out from the set of root objects
  if (KJScriptImp::hook) {
    KJScriptImp *scr = KJScriptImp::hook;
    do {
      scr->mark();
      scr = scr->next;
    } while (scr != KJScriptImp::hook);
  }

  // mark any other objects that we wouldn't delete anyway
  block = root;
  while (block) {
    Imp **r = (Imp**)block->mem;
    assert(r);
    for (int i = 0; i < block->size; i++, r++)
      if (*r && (*r)->created() && ((*r)->refcount || !(*r)->gcAllowed()) && !(*r)->marked())
        (*r)->mark();
    block = block->next;
  }

  // SWEEP: delete everything with a zero refcount (garbage)
  block = root;
  while (block) {
    Imp **r = (Imp**)block->mem;
    int del = 0;
    for (int i = 0; i < block->size; i++, r++) {
      if (*r && ((*r)->refcount == 0) && !(*r)->marked() && (*r)->gcAllowed()) {
	// emulate destructing part of 'operator delete()'
	(*r)->~Imp();
	free(*r);
	*r = 0L;
	del++;
      }
    }
    filled -= del;
    block->filled -= del;
    block = block->next;
  }

  // delete the emtpy containers
  block = root;
  while (block) {
    CollectorBlock *next = block->next;
    if (block->filled == 0) {
      if (block->prev)
	block->prev->next = next;
      if (block == root)
	root = next;
      if (next)
	next->prev = block->prev;
      if (block == currentBlock) // we don't want a dangling pointer
	currentBlock = 0L;
      assert(block != root);
      delete block;
    }
    block = next;
  }

#ifdef KJS_DEBUG_MEM
  collecting = false;
#endif
}
