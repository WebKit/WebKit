//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_ATTRIBUTEMAP_H_
#define LIBANGLE_ATTRIBUTEMAP_H_


#include <EGL/egl.h>

#include <map>
#include <vector>

namespace egl
{

class AttributeMap final
{
  public:
    AttributeMap();
    AttributeMap(const AttributeMap &other);
    ~AttributeMap();

    void insert(EGLAttrib key, EGLAttrib value);
    bool contains(EGLAttrib key) const;
    EGLAttrib get(EGLAttrib key) const;
    EGLAttrib get(EGLAttrib key, EGLAttrib defaultValue) const;
    EGLint getAsInt(EGLAttrib key) const;
    EGLint getAsInt(EGLAttrib key, EGLint defaultValue) const;
    bool isEmpty() const;
    std::vector<EGLint> toIntVector() const;

    typedef std::map<EGLAttrib, EGLAttrib>::const_iterator const_iterator;

    const_iterator begin() const;
    const_iterator end() const;

    static AttributeMap CreateFromIntArray(const EGLint *attributes);
    static AttributeMap CreateFromAttribArray(const EGLAttrib *attributes);

  private:
    std::map<EGLAttrib, EGLAttrib> mAttributes;
};

}

#endif   // LIBANGLE_ATTRIBUTEMAP_H_
