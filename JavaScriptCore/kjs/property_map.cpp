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


#include "property_map.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

using namespace KJS;

// ------------------------------ PropertyMapNode ------------------------------

void PropertyMapNode::setLeft(PropertyMapNode *newLeft)
{
  if (left)
    left->setParent(0);
  left = newLeft;
  if (left)
    left->setParent(this);
}

void PropertyMapNode::setRight(PropertyMapNode *newRight)
{
  if (right)
    right->setParent(0);
  right = newRight;
  if (right)
    right->setParent(this);
}

void PropertyMapNode::setParent(PropertyMapNode *newParent)
{
  if (parent) {
    //assert(this == parent->left || this == parent->right);
    if (this == parent->left)
      parent->left = 0;
    else
      parent->right = 0;
  }
  parent = newParent;
}

PropertyMapNode *PropertyMapNode::findMax()
{
  PropertyMapNode *max = this;
  while (max->right)
    max = max->right;
  return max;
}

PropertyMapNode *PropertyMapNode::findMin()
{
  PropertyMapNode *min = this;
  while (min->left)
    min = min->left;
  return min;
}

PropertyMapNode *PropertyMapNode::next()
{
  // find the next node, going from lowest name to highest name

  // We have a right node. Starting from our right node, keep going left as far as we can
  if (right) {
    PropertyMapNode *n = right;
    while (n->left)
      n = n->left;
    return n;
  }

  // We have no right node. Keep going up to the left, until we find a node that has a parent
  // to the right. If we find one, go to it. Otherwise, we have reached the end.
  PropertyMapNode *n = this;
  while (n->parent && n->parent->right == n) {
    // parent is to our left
    n = n->parent;
  }
  if (n->parent && n->parent->left == n) {
    // parent is to our right
    return n->parent;
  }

  return 0;
}

// ------------------------------ PropertyMap ----------------------------------

int KJS::uscompare(const UString &s1, const UString &s2)
{
  int len1 = s1.size();
  int len2 = s2.size();
  if (len1 < len2)
    return -1;
  else if (len1 > len2)
    return 1;
  else
    return memcmp(s1.data(), s2.data(), len1*sizeof(UChar));
}

PropertyMap::PropertyMap()
{
  root = 0;
}

PropertyMap::~PropertyMap()
{
}

void PropertyMap::put(const UString &name, ValueImp *value, int attr)
{
  // if not root; make the root the new node
  if (!root) {
    root = new PropertyMapNode(name, value, attr, 0);
    return;
  }

  // try to find the parent node
  PropertyMapNode *parent = root;
  int isLeft = false;
  while (true) {
    int cmp = uscompare(name, parent->name);
    if (cmp < 0) {
      // traverse to left node (if any)
      isLeft = true;
      if (!parent->left)
        break;
      else
        parent = parent->left;
    }
    else if (cmp > 0) {
      // traverse to right node (if any)
      isLeft = false;
      if (!parent->right)
        break;
      else
        parent = parent->right;
    }
    else {
      // a node with this name already exists; just replace the value
      parent->value = value;
      return;
    }
  }

  // we found the parent
  //assert(parent);

  if (isLeft) {
    //assert(!parent->left);
    parent->left = new PropertyMapNode(name, value, attr, parent);
  }
  else {
    //assert(!parent->right);
    parent->right = new PropertyMapNode(name, value, attr, parent);
  }
  updateHeight(parent);


  PropertyMapNode *node = parent;
  while (node) {
    PropertyMapNode *p = node->parent;
    balance(node); // may change node
    node = p;
  }
}

void PropertyMap::remove(const UString &name)
{
  PropertyMapNode *node = getNode(name);
  if (!node) // name not in tree
    return;

  PropertyMapNode *removed = remove(node);
  if (removed)
    delete node;
}

ValueImp *PropertyMap::get(const UString &name) const
{
  const PropertyMapNode *n = getNode(name);
  return n ? n->value : 0;
}

void PropertyMap::clear(PropertyMapNode *node)
{
  if (node == 0)
    node = root;
  if (node == 0) // nothing to do
    return;

  if (node->left)
    clear(node->left);
  if (node->right)
    clear(node->right);
  if (node == root)
    root = 0;
  delete node;
}

void PropertyMap::dump(const PropertyMapNode *node, int indent) const
{
  if (!node && indent > 0)
    return;
  if (!node)
    node = root;
  if (!node)
    return;

  assert(!node->right || node->right->parent == node);
  dump(node->right, indent+1);
  for (int i = 0; i < indent; i++) {
    printf("    ");
  }
  printf("[%d] %s\n", node->height, node->name.ascii());
  assert(!node->left || node->left->parent == node);
  dump(node->left, indent+1);
}

void PropertyMap::checkTree(const PropertyMapNode *node) const
{
  if (!root)
    return;
  if (node == 0)
    node = root;
  if (node == root) {
    assert(!node->parent);
  }
  assert(!node->right || node->right->parent == node);
  assert(!node->left || node->left->parent == node);
  assert(node->left != node);
  assert(node->right != node);
  if (node->left && node->right)
    assert(node->left != node->right);

  PropertyMapNode *n = node->parent;
  while (n) {
    assert(n != node);
    n = n->parent;
  }

  if (node->right)
    checkTree(node->right);
  if (node->left)
    checkTree(node->left);
}

PropertyMapNode *PropertyMap::getNode(const UString &name) const
{
  PropertyMapNode *node = root;

  while (true) {
    if (!node)
      return 0;

    int cmp = uscompare(name, node->name);
    if (cmp < 0)
      node = node->left;
    else if (cmp > 0)
      node = node->right;
    else
      return node;
  }
}

PropertyMapNode *PropertyMap::first() const
{
  if (!root)
    return 0;

  PropertyMapNode *node = root;
  while (node->left)
    node = node->left;
  return node;
}

PropertyMapNode * PropertyMap::remove(PropertyMapNode *node)
{
  PropertyMapNode *parent = node->parent;
  //assert(!parent || (node == parent->left || node == parent->right));
  bool isLeft = (parent && node == parent->left);

  PropertyMapNode *replace = 0;
  if (node->left && node->right) {
    PropertyMapNode *maxLeft = node->left->findMax();
    if (maxLeft == node->left) {
      maxLeft->setRight(node->right);
      replace = maxLeft;
    }
    else {
      remove(maxLeft);

      maxLeft->setLeft(node->left);
      maxLeft->setRight(node->right);
      replace = maxLeft;
    }
    // removing maxLeft could have re-balanced the tree, so recalculate
    // parent again
    parent = node->parent;
    //assert(!parent || (node == parent->left || node == parent->right));
    isLeft = (parent && node == parent->left);
  }
  else if (node->left) {
    replace = node->left;
  }
  else if (node->right) {
    replace = node->right;
  }
  else {
    replace = 0;
  }

  if (parent) {
    if (isLeft)
      parent->setLeft(replace);
    else
      parent->setRight(replace);
  }
  else {
    root = replace;
    if (replace)
      replace->parent = 0;
  }

  if (replace)
    updateHeight(replace); // will also update parent's height
  else if (parent)
    updateHeight(parent);
  else if (root)
    updateHeight(root);


  // balance the tree
  PropertyMapNode *bal = parent;
  while (bal) {
    PropertyMapNode *p = bal->parent;
    balance(bal); // may change bal
    bal = p;
  }

  return node;
}

void PropertyMap::balance(PropertyMapNode* node)
{
  int lheight = node->left ? node->left->height : 0;
  int rheight = node->right ? node->right->height : 0;

  int bal = rheight-lheight;

  if (bal < -1) {
    //assert(node->left);
    int llheight = node->left->left ? node->left->left->height : 0;
    int lrheight = node->left->right ? node->left->right->height : 0;
    int lbal = lrheight - llheight;

    if (lbal < 0) {
      rotateLL(node); // LL rotation
    }
    else {
      // lbal >= 0
      rotateLR(node);
    }
  }
  else if (bal > 1) {
    int rlheight = node->right->left ? node->right->left->height : 0;
    int rrheight = node->right->right ? node->right->right->height : 0;
    int rbal = rrheight - rlheight;
    if (rbal < 0) {
      rotateRL(node);
    }
    else {
      // rbal >= 0
      rotateRR(node); // RR rotateion
    }
  }
}

void PropertyMap::updateHeight(PropertyMapNode* &node)
{
  int lheight = node->left ? node->left->height : 0;
  int rheight = node->right ? node->right->height : 0;
  if (lheight > rheight)
    node->height = lheight+1;
  else
    node->height = rheight+1;
  //assert(node->parent != node);
  if (node->parent)
    updateHeight(node->parent);
}

void PropertyMap::rotateRR(PropertyMapNode* &node)
{
  /*
    Perform a RR ("single left") rotation, e.g.

      a                b
     / \              / \
    X   b     -->    a   Z
       / \          / \
      Y   Z        X   Y
  */

  // Here, node is initially a, will be replaced with b

  PropertyMapNode *a = node;
  PropertyMapNode *b = node->right;

  PropertyMapNode *parent = a->parent;
  //assert(!parent || (a == parent->left || a == parent->right));
  bool isLeft = (parent && a == parent->left);

  // do the rotation
  a->setRight(b->left);
  b->setLeft(a);

  // now node is b
  node = b;
  if (parent) {
    if (isLeft)
      parent->setLeft(b);
    else
      parent->setRight(b);
  }
  else {
    // a was the root node
    root = b;
  }

  updateHeight(a);
  updateHeight(b);
}


void PropertyMap::rotateLL(PropertyMapNode* &node)
{
  /*
    Perform a LL ("single right") rotation, e.g.


        a              b
       / \            / \
      b   Z   -->    X   a
     / \                / \
    X   Y              Y   Z
  */

  // Here, node is initially a, will be replaced with b

  PropertyMapNode *a = node;
  PropertyMapNode *b = node->left;

  PropertyMapNode *parent = a->parent;
  //assert(!parent || (a == parent->left || a == parent->right));
  bool isLeft = (parent && a == parent->left);

  // do the rotation
  a->setLeft(b->right);
  b->setRight(a);

  // now node is b
  node = b;
  if (parent) {
    if (isLeft)
      parent->setLeft(b);
    else
      parent->setRight(b);
  }
  else {
    // a was the root node
    root = b;
  }

  updateHeight(a);
  updateHeight(b);
}

void PropertyMap::rotateRL(PropertyMapNode* &node)
{
  /*
    Perform a RL ("double left") rotation, e.g.

        a                   a                          b
      /   \      LL on c  /   \          RR on b     /   \
     W      c      -->   W      b          -->     a       c
           / \                 / \                / \     / \
          b   Z               X   c              W   X   Y   Z
         / \                     / \
        X   Y                   Y   Z

  */

  PropertyMapNode *a = node;
  PropertyMapNode *c = node->right;
  PropertyMapNode *b = node->right->left;

  rotateLL(c);
  rotateRR(b);

  updateHeight(a);
  updateHeight(c);
  updateHeight(b);
}

void PropertyMap::rotateLR(PropertyMapNode* &node)
{
  /*
    Perform a LR ("double right") rotation, e.g.

          a                         a                      b
        /   \    RR on c          /   \     LL on a      /   \
      c       Z    -->          b       Z     -->      c       a
     / \                       / \                    / \     / \
    W   b                     c   Y                  W   X   Y   Z
       / \                   / \
      X   Y                 W   X
  */

  PropertyMapNode *a = node;
  PropertyMapNode *c = node->left;
  PropertyMapNode *b = node->left->right;

  rotateRR(c);
  rotateLL(a);

  updateHeight(c);
  updateHeight(a);
  updateHeight(b);
}
