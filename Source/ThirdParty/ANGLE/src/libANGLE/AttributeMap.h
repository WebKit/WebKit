//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_ATTRIBUTEMAP_H_
#define LIBANGLE_ATTRIBUTEMAP_H_

#include "common/PackedEnums.h"

#include <EGL/egl.h>

#include <functional>
#include <map>
#include <vector>

namespace egl
{
class Display;
struct ValidationContext;

// Validates {key, value} for each attribute. Generates an error and returns false on invalid usage.
using AttributeValidationFunc =
    std::function<bool(const ValidationContext *, const Display *, EGLAttrib)>;

class AttributeMap final
{
  public:
    AttributeMap();
    AttributeMap(const AttributeMap &other);
    AttributeMap &operator=(const AttributeMap &other);
    ~AttributeMap();

    void insert(EGLAttrib key, EGLAttrib value);
    bool contains(EGLAttrib key) const;

    EGLAttrib get(EGLAttrib key) const;
    EGLAttrib get(EGLAttrib key, EGLAttrib defaultValue) const;
    EGLint getAsInt(EGLAttrib key) const;
    EGLint getAsInt(EGLAttrib key, EGLint defaultValue) const;

    template <typename PackedEnumT>
    PackedEnumT getAsPackedEnum(EGLAttrib key) const
    {
        return FromEGLenum<PackedEnumT>(static_cast<EGLenum>(get(key)));
    }

    template <typename PackedEnumT>
    PackedEnumT getAsPackedEnum(EGLAttrib key, PackedEnumT defaultValue) const
    {
        auto iter = attribs().find(key);
        return (attribs().find(key) != attribs().end())
                   ? FromEGLenum<PackedEnumT>(static_cast<EGLenum>(iter->second))
                   : defaultValue;
    }

    bool isEmpty() const;
    std::vector<EGLint> toIntVector() const;

    typedef std::map<EGLAttrib, EGLAttrib>::const_iterator const_iterator;

    const_iterator begin() const;
    const_iterator end() const;

    [[nodiscard]] bool validate(const ValidationContext *val,
                                const egl::Display *display,
                                AttributeValidationFunc validationFunc) const;

    // TODO: remove this and validate at every call site. http://anglebug.com/6671
    void initializeWithoutValidation() const;

    static AttributeMap CreateFromIntArray(const EGLint *attributes);
    static AttributeMap CreateFromAttribArray(const EGLAttrib *attributes);

  private:
    bool isValidated() const;

    const std::map<EGLAttrib, EGLAttrib> &attribs() const
    {
        ASSERT(isValidated());
        return mValidatedAttributes;
    }

    std::map<EGLAttrib, EGLAttrib> &attribs()
    {
        ASSERT(isValidated());
        return mValidatedAttributes;
    }

    mutable const EGLint *mIntPointer       = nullptr;
    mutable const EGLAttrib *mAttribPointer = nullptr;
    mutable std::map<EGLAttrib, EGLAttrib> mValidatedAttributes;
};
}  // namespace egl

#endif  // LIBANGLE_ATTRIBUTEMAP_H_
