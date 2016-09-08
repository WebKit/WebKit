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
#include <cstdlib>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaConstraint : public RefCounted<MediaConstraint> {
public:
    static Ref<MediaConstraint> create(const String&);

    enum class ConstraintType { ExactConstraint, IdealConstraint, MinConstraint, MaxConstraint };

    virtual ~MediaConstraint() { };

    virtual Ref<MediaConstraint> copy() const;
    virtual bool isEmpty() const = 0;
    virtual bool isMandatory() const = 0;

    virtual bool getMin(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(int&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool validForRange(int, int) const { ASSERT_NOT_REACHED(); return false; }
    virtual int find(std::function<bool(ConstraintType, int)>) const { ASSERT_NOT_REACHED(); return 0; }
    virtual double fitnessDistance(int, int) const { ASSERT_NOT_REACHED(); return 0; }

    virtual bool getMin(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(double&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool validForRange(double, double) const { ASSERT_NOT_REACHED(); return false; }
    virtual double find(std::function<bool(ConstraintType, double)>) const { ASSERT_NOT_REACHED(); return 0; }
    virtual double fitnessDistance(double, double) const { ASSERT_NOT_REACHED(); return 0; }

    virtual bool getMin(bool&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(bool&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(bool&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(bool&) const { ASSERT_NOT_REACHED(); return false; }
    virtual double fitnessDistance(bool) const { ASSERT_NOT_REACHED(); return 0; }

    virtual bool getMin(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getMax(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getExact(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual bool getIdeal(Vector<String>&) const { ASSERT_NOT_REACHED(); return false; }
    virtual const String& find(std::function<bool(ConstraintType, const String&)>) const { ASSERT_NOT_REACHED(); return emptyString(); }

    virtual double fitnessDistance(const String&) const { ASSERT_NOT_REACHED(); return 0; }
    virtual double fitnessDistance(const Vector<String>&) const { ASSERT_NOT_REACHED(); return 0; }

    virtual void merge(const MediaConstraint&) { ASSERT_NOT_REACHED(); }

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

    bool nearlyEqual(double a, double b) const
    {
        // Don't require strict equality when comparing constraints, or many floating point constraint values,
        // e.g. "aspectRatio: 1.333", will never match.
        const double epsilon = 0.00001;
        return std::abs(a - b) <= epsilon;
    }

    double fitnessDistance(ValueType rangeMin, ValueType rangeMax) const final {
        // https://w3c.github.io/mediacapture-main/#dfn-applyconstraints
        // 1. If the constraint is not supported by the browser, the fitness distance is 0.
        if (isEmpty())
            return 0;

        // 2. If the constraint is required ('min', 'max', or 'exact'), and the settings
        //    dictionary's value for the constraint does not satisfy the constraint, the
        //    fitness distance is positive infinity.
        bool valid = validForRange(rangeMin, rangeMax);
        if (m_exact && !valid)
            return std::numeric_limits<double>::infinity();

        if (m_min && !valid)
            return std::numeric_limits<double>::infinity();

        if (m_max && !valid)
            return std::numeric_limits<double>::infinity();

        // 3. If no ideal value is specified, the fitness distance is 0.
        if (!m_ideal)
            return 0;

        // 4. For all positive numeric non-required constraints (such as height, width, frameRate,
        //    aspectRatio, sampleRate and sampleSize), the fitness distance is the result of the formula
        //
        //         (actual == ideal) ? 0 : |actual - ideal| / max(|actual|,|ideal|)
        ValueType ideal = m_ideal.value();
        if (ideal >= rangeMin && ideal <= rangeMax)
            return 0;

        ideal = ideal > std::max(rangeMin, rangeMax) ? rangeMax : rangeMin;
        return static_cast<double>(std::abs(ideal - m_ideal.value())) / std::max(std::abs(ideal), std::abs(m_ideal.value()));
    }

    void merge(const MediaConstraint& other) final {
        if (other.isEmpty())
            return;

        ValueType value;
        if (other.getExact(value))
            m_exact = value;

        if (other.getMin(value))
            m_min = value;

        if (other.getMax(value))
            m_max = value;

        // https://w3c.github.io/mediacapture-main/#constrainable-interface
        // When processing advanced constraints:
        //   ... the User Agent must attempt to apply, individually, any 'ideal' constraints or
        //   a constraint given as a bare value for the property. Of these properties, it must
        //   satisfy the largest number that it can, in any order.
        if (other.getIdeal(value)) {
            if (!m_ideal || value > m_ideal.value())
                m_ideal = value;
        }
    }

    bool validForRange(ValueType rangeMin, ValueType rangeMax) const final {
        if (isEmpty())
            return false;

        if (m_exact) {
            const ValueType exact = m_exact.value();
            if (exact < rangeMin && !nearlyEqual(exact, rangeMin))
                return false;
            if (exact > rangeMax && !nearlyEqual(exact, rangeMax))
                return false;
        }

        if (m_min) {
            const ValueType constraintMin = m_min.value();
            if (constraintMin > rangeMax && !nearlyEqual(constraintMin, rangeMax))
                return false;
        }


        if (m_max) {
            const ValueType constraintMax = m_max.value();
            if (constraintMax < rangeMin && !nearlyEqual(constraintMax, rangeMin))
                return false;
        }

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

    Optional<ValueType> m_min;
    Optional<ValueType> m_max;
    Optional<ValueType> m_exact;
    Optional<ValueType> m_ideal;
};

class IntConstraint final : public NumericConstraint<int> {
public:
    static Ref<IntConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new IntConstraint(name, type)); }

    Ref<MediaConstraint> copy() const final;

private:
    explicit IntConstraint(const String& name, MediaConstraintType type)
        : NumericConstraint<int>(name, type)
    {
    }
};

class DoubleConstraint final : public NumericConstraint<double> {
public:
    static Ref<DoubleConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new DoubleConstraint(name, type)); }

    Ref<MediaConstraint> copy() const final;

private:
    explicit DoubleConstraint(const String& name, MediaConstraintType type)
        : NumericConstraint<double>(name, type)
    {
    }
};

class BooleanConstraint final : public MediaConstraint {
public:
    static Ref<BooleanConstraint> create(const String& name, MediaConstraintType type) { return adoptRef(*new BooleanConstraint(name, type)); }

    Ref<MediaConstraint> copy() const final;

    void setExact(bool value) { m_exact = value; }
    void setIdeal(bool value) { m_ideal = value; }

    bool getExact(bool&) const final;
    bool getIdeal(bool&) const final;

    bool isEmpty() const final { return !m_exact && !m_ideal; };
    bool isMandatory() const final { return bool(m_exact); }

    double fitnessDistance(bool value) const final {
        // https://w3c.github.io/mediacapture-main/#dfn-applyconstraints
        // 1. If the constraint is not supported by the browser, the fitness distance is 0.
        if (isEmpty())
            return 0;

        // 2. If the constraint is required ('min', 'max', or 'exact'), and the settings
        //    dictionary's value for the constraint does not satisfy the constraint, the
        //    fitness distance is positive infinity.
        if (m_exact && value != m_exact.value())
            return std::numeric_limits<double>::infinity();

        // 3. If no ideal value is specified, the fitness distance is 0.
        if (!m_ideal || m_ideal.value() == value)
            return 0;

        // 5. For all string and enum non-required constraints (deviceId, groupId, facingMode,
        // echoCancellation), the fitness distance is the result of the formula
        //        (actual == ideal) ? 0 : 1
        return 1;
    }

    void merge(const MediaConstraint& other) final {
        if (other.isEmpty())
            return;

        bool value;
        if (other.getExact(value))
            m_exact = value;

        if (other.getIdeal(value)) {
            if (!m_ideal || (value && !m_ideal.value()))
                m_ideal = value;
        }
    }

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

    Ref<MediaConstraint> copy() const final;

    void setExact(const String&);
    void appendExact(const String&);
    void setIdeal(const String&);
    void appendIdeal(const String&);

    bool getExact(Vector<String>&) const final;
    bool getIdeal(Vector<String>&) const final;

    bool isEmpty() const final { return m_exact.isEmpty() && m_ideal.isEmpty(); }
    bool isMandatory() const final { return !m_exact.isEmpty(); }

    double fitnessDistance(const String&) const final;
    double fitnessDistance(const Vector<String>&) const final;

    const String& find(std::function<bool(ConstraintType, const String&)>) const final;
    void merge(const MediaConstraint&) final;

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

class FlattenedConstraint {
public:
    typedef Vector<RefPtr<MediaConstraint>>::const_iterator const_iterator;

    void set(const MediaConstraint&);
    void merge(const MediaConstraint&);
    bool isEmpty() const { return m_constraints.isEmpty(); }

    const_iterator begin() const { return m_constraints.begin(); }
    const_iterator end() const { return m_constraints.end(); }

private:

    Vector<RefPtr<MediaConstraint>> m_constraints;
};

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
