// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
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
 *  $Id$
 */


#ifndef _KJS_PROPERTY_MAP_H_
#define _KJS_PROPERTY_MAP_H_

#include "ustring.h"
#include "value.h"

namespace KJS {

  class PropertyMapNode {
  public:
    PropertyMapNode(const UString &n, ValueImp *v, int att, PropertyMapNode *p)
      : name(n), value(v), attr(att), left(0), right(0), parent(p), height(1) {}

    UString name;
    ValueImp *value;
    int attr;

    void setLeft(PropertyMapNode *newLeft);
    void setRight(PropertyMapNode *newRight);
    PropertyMapNode *findMax();
    PropertyMapNode *findMin();

    PropertyMapNode *next();

    PropertyMapNode *left;
    PropertyMapNode *right;
    PropertyMapNode *parent;
    int height;

  private:
    void setParent(PropertyMapNode *newParent);
  };

  /**
   * @internal
   *
   * Provides a name/value map for storing properties based on UString keys. The
   * map is implemented using an AVL tree, which provides O(log2 n) performance
   * for insertion and deletion, and retrieval.
   *
   * For a description of AVL tree operations, see
   * http://www.cis.ksu.edu/~howell/300f99/minavl/rotations.html
   * http://www.cgc.cs.jhu.edu/~jkloss/htmls/structures/avltree.html
   */
  class PropertyMap {
  public:
    PropertyMap();
    ~PropertyMap();

    void put(const UString &name, ValueImp *value, int attr);
    void remove(const UString &name);
    ValueImp *get(const UString &name) const;

    void clear(PropertyMapNode *node = 0);
    void dump(const PropertyMapNode *node = 0, int indent = 0) const;
    void checkTree(const PropertyMapNode *node = 0) const;

    PropertyMapNode *getNode(const UString &name) const;
    PropertyMapNode *first() const;

  private:

    PropertyMapNode *remove(PropertyMapNode *node);
    void balance(PropertyMapNode* node);
    void updateHeight(PropertyMapNode* &node);

    void rotateRR(PropertyMapNode* &node);
    void rotateLL(PropertyMapNode* &node);
    void rotateRL(PropertyMapNode* &node);
    void rotateLR(PropertyMapNode* &node);

    PropertyMapNode *root;
  };

  int uscompare(const UString &s1, const UString &s2);

}; // namespace

#endif // _KJS_PROPERTY_MAP_H_
