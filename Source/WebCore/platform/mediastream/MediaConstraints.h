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

#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Dictionary;

class BaseConstraint : public RefCounted<BaseConstraint> {
public:
    static RefPtr<BaseConstraint> create(const String& name, const Dictionary&);

    virtual ~BaseConstraint() { };
    virtual bool isEmpty() const = 0;
    virtual bool isMandatory() const = 0;

    virtual bool hasIdeal() const = 0;

    String name() const { return m_name; }

protected:
    explicit BaseConstraint(const String& name)
        : m_name(name)
    {
    }

private:
    static RefPtr<BaseConstraint> createEmptyDerivedConstraint(const String& name);

    virtual void initializeWithDictionary(const Dictionary&) = 0;

    String m_name;
};

class NumericConstraint : public BaseConstraint {
public:
    void setHasMin(bool value) { m_hasMin = value; }
    void setHasMax(bool value) { m_hasMax = value; }
    void setHasExact(bool value) { m_hasExact = value; }
    void setHasIdeal(bool value) { m_hasIdeal = value; }

    bool hasMin() const { return m_hasMin; }
    bool hasMax() const { return m_hasMax; }
    bool hasExact() const { return m_hasExact; }
    bool hasIdeal() const override { return m_hasIdeal; }

    bool isEmpty() const override { return !m_hasMin && !m_hasMax && !m_hasExact && !m_hasIdeal; }
    bool isMandatory() const override { return m_hasMin || m_hasMax || m_hasExact; }

protected:
    explicit NumericConstraint(const String& name)
        : BaseConstraint(name)
    {
    }

private:
    bool m_hasMin { false };
    bool m_hasMax { false };
    bool m_hasExact { false };
    bool m_hasIdeal { false };
};

class LongConstraint final : public NumericConstraint {
public:
    static Ref<LongConstraint> create(const String& name);

    void setMin(long value);
    void setMax(long value);
    void setExact(long value);
    void setIdeal(long value);

    long min() const { return m_min; }
    long max() const { return m_max; }
    long exact() const { return m_exact; }
    long ideal() const { return m_ideal; }
    
private:
    explicit LongConstraint(const String& name)
        : WebCore::NumericConstraint(name)
    {
    }

    void initializeWithDictionary(const Dictionary&) override;

    long m_min;
    long m_max;
    long m_exact;
    long m_ideal;
};

class DoubleConstraint final : public NumericConstraint {
public:
    static Ref<DoubleConstraint> create(const String& name);

    void setMin(double value);
    void setMax(double value);
    void setExact(double value);
    void setIdeal(double value);

    double min() const { return m_min; }
    double max() const { return m_max; }
    double exact() const { return m_exact; }
    double ideal() const { return m_ideal; }

private:
    explicit DoubleConstraint(const String& name)
        : WebCore::NumericConstraint(name)
    {
    }

    void initializeWithDictionary(const Dictionary&) override;

    double m_min;
    double m_max;
    double m_exact;
    double m_ideal;
};

class BooleanConstraint final : public BaseConstraint {
public:
    static Ref<BooleanConstraint> create(const String& name);

    void setExact(bool value);
    void setIdeal(bool value);

    bool exact() const { return m_exact; }
    bool ideal() const { return m_ideal; }
    bool hasExact() const { return m_hasExact; }
    bool hasIdeal() const override { return m_hasIdeal; }

    bool isEmpty() const override { return !m_hasExact && !m_hasIdeal; };
    bool isMandatory() const override { return m_hasExact; }

private:
    explicit BooleanConstraint(const String& name)
        : BaseConstraint(name)
    {
    }

    void initializeWithDictionary(const Dictionary&) override;

    bool m_exact { false };
    bool m_ideal { false };
    bool m_hasExact { false };
    bool m_hasIdeal { false };
};

class StringConstraint final : public BaseConstraint {
public:
    static Ref<StringConstraint> create(const String& name);

    void setExact(const String&);
    void setIdeal(const String&);

    const String& exact() const { return m_exact; }
    const String& ideal() const { return m_ideal; }

    bool hasExact() const { return !m_exact.isEmpty(); }
    bool hasIdeal() const override { return !m_ideal.isEmpty(); }

    bool isEmpty() const override { return m_exact.isEmpty() && m_ideal.isEmpty(); }
    bool isMandatory() const override { return !m_exact.isEmpty(); }

private:
    explicit StringConstraint(const String& name)
        : BaseConstraint(name)
    {
    }

    void initializeWithDictionary(const Dictionary&) override;

    String m_exact;
    String m_ideal;
};

struct MediaConstraint {
    MediaConstraint(String name, String value)
        : m_name(name)
        , m_value(value)
    {
    }

    String m_name;
    String m_value;
};

class MediaConstraints : public RefCounted<MediaConstraints> {
public:
    virtual ~MediaConstraints() { }

    virtual void getMandatoryConstraints(Vector<MediaConstraint>&) const = 0;
    virtual void getOptionalConstraints(Vector<MediaConstraint>&) const = 0;

    virtual bool getMandatoryConstraintValue(const String& name, String& value) const = 0;
    virtual bool getOptionalConstraintValue(const String& name, String& value) const = 0;

protected:
    MediaConstraints() { }
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaConstraints_h
