/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaConstraints_h
#define MediaConstraints_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaConstraint : public RefCounted<MediaConstraint> {
public:
    static RefPtr<MediaConstraint> create(const String& name);

    virtual ~MediaConstraint() { };
    virtual bool isEmpty() const = 0;
    virtual bool isMandatory() const = 0;

    virtual bool getMin(int&) const;
    virtual bool getMax(int&) const;
    virtual bool getExact(int&) const;
    virtual bool getIdeal(int&) const;

    virtual bool getMin(double&) const;
    virtual bool getMax(double&) const;
    virtual bool getExact(double&) const;
    virtual bool getIdeal(double&) const;

    virtual bool getMin(bool&) const;
    virtual bool getMax(bool&) const;
    virtual bool getExact(bool&) const;
    virtual bool getIdeal(bool&) const;

    virtual bool getMin(Vector<String>&) const;
    virtual bool getMax(Vector<String>&) const;
    virtual bool getExact(Vector<String>&) const;
    virtual bool getIdeal(Vector<String>&) const;

    MediaConstraintType type() const { return m_type; }


protected:
    explicit MediaConstraint(MediaConstraintType type)
        : m_type(type)
    {
    }

private:
    MediaConstraintType m_type;
};

class NumericConstraint : public MediaConstraint {
public:
    bool isEmpty() const override { return !m_hasMin && !m_hasMax && !m_hasExact && !m_hasIdeal; }
    bool isMandatory() const override { return m_hasMin || m_hasMax || m_hasExact; }

protected:
    explicit NumericConstraint(MediaConstraintType type)
        : MediaConstraint(type)
    {
    }

    void setHasMin(bool value) { m_hasMin = value; }
    void setHasMax(bool value) { m_hasMax = value; }
    void setHasExact(bool value) { m_hasExact = value; }
    void setHasIdeal(bool value) { m_hasIdeal = value; }

    bool hasMin() const { return m_hasMin; }
    bool hasMax() const { return m_hasMax; }
    bool hasExact() const { return m_hasExact; }
    bool hasIdeal() const { return m_hasIdeal; }

private:
    bool m_hasMin { false };
    bool m_hasMax { false };
    bool m_hasExact { false };
    bool m_hasIdeal { false };
};

class IntConstraint final : public NumericConstraint {
public:
    static Ref<IntConstraint> create(MediaConstraintType);

    void setMin(int value);
    void setMax(int value);
    void setExact(int value);
    void setIdeal(int value);

    bool getMin(int&) const final;
    bool getMax(int&) const final;
    bool getExact(int&) const final;
    bool getIdeal(int&) const final;

private:
    explicit IntConstraint(MediaConstraintType type)
        : WebCore::NumericConstraint(type)
    {
    }

    int m_min;
    int m_max;
    int m_exact;
    int m_ideal;
};

class DoubleConstraint final : public NumericConstraint {
public:
    static Ref<DoubleConstraint> create(MediaConstraintType);

    void setMin(double value);
    void setMax(double value);
    void setExact(double value);
    void setIdeal(double value);

    bool getMin(double&) const final;
    bool getMax(double&) const final;
    bool getExact(double&) const final;
    bool getIdeal(double&) const final;

private:
    explicit DoubleConstraint(MediaConstraintType type)
        : WebCore::NumericConstraint(type)
    {
    }

    double m_min;
    double m_max;
    double m_exact;
    double m_ideal;
};

class BooleanConstraint final : public MediaConstraint {
public:
    static Ref<BooleanConstraint> create(MediaConstraintType);

    void setExact(bool value);
    void setIdeal(bool value);

    bool getExact(bool&) const final;
    bool getIdeal(bool&) const final;

    bool isEmpty() const final { return !m_hasExact && !m_hasIdeal; };
    bool isMandatory() const final { return m_hasExact; }

private:
    explicit BooleanConstraint(MediaConstraintType type)
        : MediaConstraint(type)
    {
    }

    bool m_exact { false };
    bool m_ideal { false };
    bool m_hasExact { false };
    bool m_hasIdeal { false };
};

class StringConstraint final : public MediaConstraint {
public:
    static Ref<StringConstraint> create(MediaConstraintType);

    void setExact(const String&);
    void appendExact(const String&);
    void setIdeal(const String&);
    void appendIdeal(const String&);

    bool getExact(Vector<String>&) const final;
    bool getIdeal(Vector<String>&) const final;

    bool isEmpty() const final { return m_exact.isEmpty() && m_ideal.isEmpty(); }
    bool isMandatory() const final { return !m_exact.isEmpty(); }

private:
    explicit StringConstraint(MediaConstraintType type)
        : MediaConstraint(type)
    {
    }

    Vector<String> m_exact;
    Vector<String> m_ideal;
};

using MediaTrackConstraintSetMap = HashMap<String, RefPtr<MediaConstraint>>;

class MediaConstraints : public RefCounted<MediaConstraints> {
public:
    virtual ~MediaConstraints() { }

    virtual const MediaTrackConstraintSetMap& mandatoryConstraints() const = 0;
    virtual const Vector<MediaTrackConstraintSetMap>& advancedConstraints() const = 0;
    virtual bool isValid() const = 0;

protected:
    MediaConstraints() { }
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaConstraints_h
