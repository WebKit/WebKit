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

    enum class ConstraintType { ExactConstraint, IdealConstraint, MinConstraint, MaxConstraint };

    virtual ~MediaConstraint() { };
    virtual bool isEmpty() const = 0;
    virtual bool isMandatory() const = 0;

    virtual bool getMin(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool validForRange(int, int) const { ASSERT_NOT_REACHED(); return false; }
    virtual int find(std::function<bool(ConstraintType, int)>) const { ASSERT_NOT_REACHED(); return 0; }

    virtual bool getMin(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool validForRange(double, double) const { ASSERT_NOT_REACHED(); return false; }
    virtual double find(std::function<bool(ConstraintType, double)>) const { ASSERT_NOT_REACHED(); return 0; }

    virtual bool getMin(bool&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(bool&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(bool&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(bool&) const { ASSERT_NOT_REACHED(); return false; }

    virtual bool getMin(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual const String& find(std::function<bool(ConstraintType, const String&)>) const { ASSERT_NOT_REACHED(); return emptyString(); }

    MediaConstraintType type() const { return m_type; }
    const String& name() const { return m_name; }

protected:
    explicit MediaConstraint(const String& name, MediaConstraintType type)
        : m_name(name)
        , m_type(type)
    {
    }

private:
    String m_name;
    MediaConstraintType m_type;
};

template<class ValueType>
class NumericConstraint : public MediaConstraint {
public:
    bool isEmpty() const override { return !m_min && !m_max && !m_exact && !m_ideal; }
    bool isMandatory() const override { return m_min || m_max || m_exact; }

    void setMin(ValueType value) { m_min = value; }
    void setMax(ValueType value) { m_max = value; }
    void setExact(ValueType value) { m_exact = value; }
    void setIdeal(ValueType value) { m_ideal = value; }

    bool getMin(ValueType& min) const final {
        if (!m_min)
            return false;

        min = m_min.value();
        return true;
    }

    bool getMax(ValueType& max) const final {
        if (!m_max)
            return false;

        max = m_max.value();
        return true;
    }

    bool getExact(ValueType& exact) const final {
        if (!m_exact)
            return false;

        exact = m_exact.value();
        return true;
    }

    bool getIdeal(ValueType& ideal) const final {
        if (!m_ideal)
            return false;

        ideal = m_ideal.value();
        return true;
    }

    bool validForRange(ValueType rangeMin, ValueType rangeMax) const final {
        if (isEmpty())
            return false;

        if (m_exact && (m_exact.value() < rangeMin || m_exact.value() > rangeMax))
            return false;

        if (m_min && m_min.value() > rangeMax)
            return false;

        if (m_max && m_max.value() < rangeMin)
            return false;

        return true;
    }

    ValueType find(std::function<bool(ConstraintType, ValueType)> function) const final {
        if (m_min && function(ConstraintType::MinConstraint, m_min.value()))
            return m_min.value();

        if (m_max && function(ConstraintType::MaxConstraint, m_max.value()))
            return m_max.value();

        if (m_exact && function(ConstraintType::ExactConstraint, m_exact.value()))
            return m_exact.value();

        if (m_ideal && function(ConstraintType::IdealConstraint, m_ideal.value()))
            return m_ideal.value();

        return 0;
    }
    

protected:
    explicit NumericConstraint(const String& name, MediaConstraintType type)
        : MediaConstraint(name, type)
    {
    }

private:
    Optional<ValueType> m_min;
    Optional<ValueType> m_max;
    Optional<ValueType> m_exact;
    Optional<ValueType> m_ideal;
};

class IntConstraint final : public NumericConstraint<int> {
public:
    static Ref<IntConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new IntConstraint(name, type)); }

private:
    explicit IntConstraint(const String& name, MediaConstraintType type)
        : NumericConstraint<int>(name, type)
    {
    }
};

class DoubleConstraint final : public NumericConstraint<double> {
public:
    static Ref<DoubleConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new DoubleConstraint(name, type)); }

private:
    explicit DoubleConstraint(const String& name, MediaConstraintType type)
        : NumericConstraint<double>(name, type)
    {
    }
};

class BooleanConstraint final : public MediaConstraint {
public:
    static Ref<BooleanConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new BooleanConstraint(name, type)); }

    void setExact(bool value) { m_exact = value; }
    void setIdeal(bool value) { m_ideal = value; }

    bool getExact(bool&) const final;
    bool getIdeal(bool&) const final;

    bool isEmpty() const final { return !m_exact && !m_ideal; };
    bool isMandatory() const final { return bool(m_exact); }

private:
    explicit BooleanConstraint(const String& name, MediaConstraintType type)
        : MediaConstraint(name, type)
    {
    }

    Optional<bool> m_exact;
    Optional<bool> m_ideal;
};

class StringConstraint final : public MediaConstraint {
public:
    static Ref<StringConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new StringConstraint(name, type)); }

    void setExact(const String&);
    void appendExact(const String&);
    void setIdeal(const String&);
    void appendIdeal(const String&);

    bool getExact(Vector<String>&) const final;
    bool getIdeal(Vector<String>&) const final;

    bool isEmpty() const final { return m_exact.isEmpty() && m_ideal.isEmpty(); }
    bool isMandatory() const final { return !m_exact.isEmpty(); }

    const String& find(std::function<bool(ConstraintType, const String&)>) const override;

private:
    explicit StringConstraint(const String& name, MediaConstraintType type)
        : MediaConstraint(name, type)
    {
    }

    Vector<String> m_exact;
    Vector<String> m_ideal;
};

class UnknownConstraint final : public MediaConstraint {
public:
    static Ref<UnknownConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new UnknownConstraint(name, type)); }

    bool isEmpty() const final { return true; }
    bool isMandatory() const final { return false; }

private:
    explicit UnknownConstraint(const String& name, MediaConstraintType type)
        : MediaConstraint(name, type)
    {
    }
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
