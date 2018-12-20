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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceSupportedConstraints.h"
#include <cstdlib>
#include <wtf/Function.h>
#include <wtf/Vector.h>

namespace WebCore {
    
class MediaConstraint {
public:
    enum class DataType { None, Integer, Double, Boolean, String };

    bool isInt() const { return m_dataType == DataType::Integer; }
    bool isDouble() const { return m_dataType == DataType::Double; }
    bool isBoolean() const { return m_dataType == DataType::Boolean; }
    bool isString() const { return m_dataType == DataType::String; }

    DataType dataType() const { return m_dataType; }
    MediaConstraintType constraintType() const { return m_constraintType; }
    const String& name() const { return m_name; }

    template <class Encoder> void encode(Encoder& encoder) const
    {
        encoder.encodeEnum(m_constraintType);
        encoder << m_name;
        encoder.encodeEnum(m_dataType);
    }

    template <class Decoder> static bool decode(Decoder& decoder, MediaConstraint& constraint)
    {
        if (!decoder.decodeEnum(constraint.m_constraintType))
            return false;

        if (!decoder.decode(constraint.m_name))
            return false;

        if (!decoder.decodeEnum(constraint.m_dataType))
            return false;

        return true;
    }

protected:
    MediaConstraint(const String& name, MediaConstraintType constraintType, DataType dataType)
        : m_name(name)
        , m_constraintType(constraintType)
        , m_dataType(dataType)
    {
    }

    MediaConstraint() = default;
    ~MediaConstraint() = default;

private:
    String m_name;
    MediaConstraintType m_constraintType { MediaConstraintType::Unknown };
    DataType m_dataType { DataType::None };
};

template<class ValueType>
class NumericConstraint : public MediaConstraint {
public:
    void setMin(ValueType value) { m_min = value; }
    void setMax(ValueType value) { m_max = value; }
    void setExact(ValueType value) { m_exact = value; }
    void setIdeal(ValueType value) { m_ideal = value; }

    bool getMin(ValueType& min) const
    {
        if (!m_min)
            return false;

        min = m_min.value();
        return true;
    }

    bool getMax(ValueType& max) const
    {
        if (!m_max)
            return false;

        max = m_max.value();
        return true;
    }

    bool getExact(ValueType& exact) const
    {
        if (!m_exact)
            return false;

        exact = m_exact.value();
        return true;
    }

    bool getIdeal(ValueType& ideal) const
    {
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

    double fitnessDistance(ValueType rangeMin, ValueType rangeMax) const
    {
        // https://w3c.github.io/mediacapture-main/#dfn-applyconstraints
        // 1. If the constraint is not supported by the browser, the fitness distance is 0.
        if (isEmpty())
            return 0;

        // 2. If the constraint is required ('min', 'max', or 'exact'), and the settings
        //    dictionary's value for the constraint does not satisfy the constraint, the
        //    fitness distance is positive infinity.
        bool valid = validForRange(rangeMin, rangeMax);
        if (m_exact) {
            if (valid && m_min && m_exact.value() < m_min.value())
                valid = false;
            if (valid && m_max && m_exact.value() > m_max.value())
                valid = false;
            if (!valid)
                return std::numeric_limits<double>::infinity();
        }

        if (m_min) {
            if (valid && m_max && m_min.value() > m_max.value())
                valid = false;
            if (!valid)
                return std::numeric_limits<double>::infinity();
        }

        if (m_max) {
            if (valid && m_min && m_max.value() < m_min.value())
                valid = false;
            if (!valid)
                return std::numeric_limits<double>::infinity();
        }

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

    double fitnessDistance(const Vector<ValueType>& discreteCapabilityValues) const
    {
        double minDistance = std::numeric_limits<double>::infinity();

        for (auto& value : discreteCapabilityValues) {
            auto distance = fitnessDistance(value, value);
            if (distance < minDistance)
                minDistance = distance;
        }

        return minDistance;
    }

    bool validForRange(ValueType rangeMin, ValueType rangeMax) const
    {
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

    ValueType find(const WTF::Function<bool(ValueType)>& function) const
    {
        if (m_min && function(m_min.value()))
            return m_min.value();

        if (m_max && function(m_max.value()))
            return m_max.value();

        if (m_exact && function(m_exact.value()))
            return m_exact.value();

        if (m_ideal && function(m_ideal.value()))
            return m_ideal.value();

        return 0;
    }

    ValueType valueForCapabilityRange(ValueType current, ValueType capabilityMin, ValueType capabilityMax) const
    {
        ValueType value { 0 };
        ValueType min { capabilityMin };
        ValueType max { capabilityMax };

        if (m_exact) {
            ASSERT(validForRange(capabilityMin, capabilityMax));
            return m_exact.value();
        }

        if (m_min) {
            value = m_min.value();
            ASSERT(validForRange(value, capabilityMax));
            if (value > min)
                min = value;
            if (value < min)
                value = min;

            // If there is no ideal, don't change if minimum is smaller than current.
            if (!m_ideal && value < current)
                value = current;
        }

        if (m_max) {
            value = m_max.value();
            ASSERT(validForRange(capabilityMin, value));
            if (value < max)
                max = value;
            if (value > max)
                value = max;
        }

        if (m_ideal)
            value = std::max(min, std::min(max, m_ideal.value()));

        return value;
    }

    ValueType valueForDiscreteCapabilityValues(ValueType current, const Vector<ValueType>& discreteCapabilityValues) const
    {
        ValueType value { 0 };
        Optional<ValueType> min;
        Optional<ValueType> max;

        if (m_exact) {
            ASSERT(discreteCapabilityValues.contains(m_exact.value()));
            return m_exact.value();
        }

        if (m_min) {
            auto index = discreteCapabilityValues.findMatching([&](ValueType value) { return m_min.value() >= value; });
            if (index != notFound) {
                min = value = discreteCapabilityValues[index];

                // If there is no ideal, don't change if minimum is smaller than current.
                if (!m_ideal && value < current)
                    value = current;
            }
        }

        if (m_max && m_max.value() >= discreteCapabilityValues[0]) {
            for (auto& discreteValue : discreteCapabilityValues) {
                if (m_max.value() <= discreteValue)
                    max = value = discreteValue;
            }
        }

        if (m_ideal && discreteCapabilityValues.contains(m_ideal.value())) {
            value = m_ideal.value();
            if (max)
                value = std::min(max.value(), value);
            if (min)
                value = std::max(min.value(), value);
        }

        return value;
    }

    bool isEmpty() const { return !m_min && !m_max && !m_exact && !m_ideal; }
    bool isMandatory() const { return m_min || m_max || m_exact; }

    template <class Encoder> void encode(Encoder& encoder) const
    {
        MediaConstraint::encode(encoder);

        encoder << m_min;
        encoder << m_max;
        encoder << m_exact;
        encoder << m_ideal;
    }

    template <class Decoder> static bool decode(Decoder& decoder, NumericConstraint& constraint)
    {
        if (!MediaConstraint::decode(decoder, constraint))
            return false;

        if (!decoder.decode(constraint.m_min))
            return false;
        if (!decoder.decode(constraint.m_max))
            return false;
        if (!decoder.decode(constraint.m_exact))
            return false;
        if (!decoder.decode(constraint.m_ideal))
            return false;
    
        return true;
    }

protected:
    NumericConstraint(const String& name, MediaConstraintType type, DataType dataType)
        : MediaConstraint(name, type, dataType)
    {
    }

    NumericConstraint() = default;

    void innerMerge(const NumericConstraint& other)
    {
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

    Optional<ValueType> m_min;
    Optional<ValueType> m_max;
    Optional<ValueType> m_exact;
    Optional<ValueType> m_ideal;
};

class IntConstraint final : public NumericConstraint<int> {
public:
    IntConstraint(const String& name, MediaConstraintType type)
        : NumericConstraint<int>(name, type, DataType::Integer)
    {
    }

    IntConstraint() = default;

    void merge(const MediaConstraint& other)
    {
        ASSERT(other.isInt());
        NumericConstraint::innerMerge(downcast<const IntConstraint>(other));
    }
};

class DoubleConstraint final : public NumericConstraint<double> {
public:
    DoubleConstraint(const String& name, MediaConstraintType type)
        : NumericConstraint<double>(name, type, DataType::Double)
    {
    }

    DoubleConstraint() = default;

    void merge(const MediaConstraint& other)
    {
        ASSERT(other.isDouble());
        NumericConstraint::innerMerge(downcast<DoubleConstraint>(other));
    }
};

class BooleanConstraint final : public MediaConstraint {
public:
    BooleanConstraint(const String& name, MediaConstraintType type)
        : MediaConstraint(name, type, DataType::Boolean)
    {
    }

    BooleanConstraint() = default;

    void setExact(bool value) { m_exact = value; }
    void setIdeal(bool value) { m_ideal = value; }

    bool getExact(bool& exact) const
    {
        if (!m_exact)
            return false;

        exact = m_exact.value();
        return true;
    }

    bool getIdeal(bool& ideal) const
    {
        if (!m_ideal)
            return false;

        ideal = m_ideal.value();
        return true;
    }

    double fitnessDistance(bool value) const
    {
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

    void merge(const MediaConstraint& other)
    {
        ASSERT(other.isBoolean());
        const BooleanConstraint& typedOther = downcast<BooleanConstraint>(other);

        if (typedOther.isEmpty())
            return;

        bool value;
        if (typedOther.getExact(value))
            m_exact = value;

        if (typedOther.getIdeal(value)) {
            if (!m_ideal || (value && !m_ideal.value()))
                m_ideal = value;
        }
    }

    bool isEmpty() const { return !m_exact && !m_ideal; };
    bool isMandatory() const { return bool(m_exact); }

    template <class Encoder> void encode(Encoder& encoder) const
    {
        MediaConstraint::encode(encoder);
        encoder << m_exact;
        encoder << m_ideal;
    }

    template <class Decoder> static bool decode(Decoder& decoder, BooleanConstraint& constraint)
    {
        if (!MediaConstraint::decode(decoder, constraint))
            return false;

        if (!decoder.decode(constraint.m_exact))
            return false;
        if (!decoder.decode(constraint.m_ideal))
            return false;

        return true;
    }

private:
    Optional<bool> m_exact;
    Optional<bool> m_ideal;
};

class StringConstraint : public MediaConstraint {
public:
    StringConstraint(const String& name, MediaConstraintType type)
        : MediaConstraint(name, type, DataType::String)
    {
    }

    StringConstraint() = default;

    void setExact(const String& value)
    {
        m_exact.clear();
        m_exact.append(value);
    }

    void appendExact(const String& value)
    {
        m_exact.append(value);
    }

    void setIdeal(const String& value)
    {
        m_ideal.clear();
        m_ideal.append(value);
    }

    void appendIdeal(const String& value)
    {
        m_ideal.append(value);
    }

    bool getExact(Vector<String>& exact) const
    {
        if (!m_exact.isEmpty())
            return false;

        exact = m_exact;
        return true;
    }

    bool getIdeal(Vector<String>& ideal) const
    {
        if (!m_ideal.isEmpty())
            return false;

        ideal = m_ideal;
        return true;
    }

    double fitnessDistance(const String&) const;
    double fitnessDistance(const Vector<String>&) const;

    const String& find(const WTF::Function<bool(const String&)>&) const;

    bool isEmpty() const { return m_exact.isEmpty() && m_ideal.isEmpty(); }
    bool isMandatory() const { return !m_exact.isEmpty(); }
    WEBCORE_EXPORT void merge(const MediaConstraint&);

    template <class Encoder> void encode(Encoder& encoder) const
    {
        MediaConstraint::encode(encoder);

        encoder << m_exact;
        encoder << m_ideal;
    }

    template <class Decoder> static bool decode(Decoder& decoder, StringConstraint& constraint)
    {
        if (!MediaConstraint::decode(decoder, constraint))
            return false;

        if (!decoder.decode(constraint.m_exact))
            return false;
        if (!decoder.decode(constraint.m_ideal))
            return false;

        return true;
    }

private:
    Vector<String> m_exact;
    Vector<String> m_ideal;
};

class UnknownConstraint final : public MediaConstraint {
public:
    UnknownConstraint(const String& name, MediaConstraintType type)
        : MediaConstraint(name, type, DataType::None)
    {
    }

private:
    bool isEmpty() const { return true; }
    bool isMandatory() const { return false; }
    void merge(const MediaConstraint&) { }
};

class MediaTrackConstraintSetMap {
public:
    WEBCORE_EXPORT void forEach(WTF::Function<void(const MediaConstraint&)>&&) const;
    void filter(const WTF::Function<bool(const MediaConstraint&)>&) const;
    bool isEmpty() const;
    WEBCORE_EXPORT size_t size() const;

    WEBCORE_EXPORT void set(MediaConstraintType, Optional<IntConstraint>&&);
    WEBCORE_EXPORT void set(MediaConstraintType, Optional<DoubleConstraint>&&);
    WEBCORE_EXPORT void set(MediaConstraintType, Optional<BooleanConstraint>&&);
    WEBCORE_EXPORT void set(MediaConstraintType, Optional<StringConstraint>&&);

    Optional<IntConstraint> width() const { return m_width; }
    Optional<IntConstraint> height() const { return m_height; }
    Optional<IntConstraint> sampleRate() const { return m_sampleRate; }
    Optional<IntConstraint> sampleSize() const { return m_sampleSize; }

    Optional<DoubleConstraint> aspectRatio() const { return m_aspectRatio; }
    Optional<DoubleConstraint> frameRate() const { return m_frameRate; }
    Optional<DoubleConstraint> volume() const { return m_volume; }

    Optional<BooleanConstraint> echoCancellation() const { return m_echoCancellation; }
    Optional<BooleanConstraint> displaySurface() const { return m_displaySurface; }
    Optional<BooleanConstraint> logicalSurface() const { return m_logicalSurface; }

    Optional<StringConstraint> facingMode() const { return m_facingMode; }
    Optional<StringConstraint> deviceId() const { return m_deviceId; }
    Optional<StringConstraint> groupId() const { return m_groupId; }

    template <class Encoder> void encode(Encoder& encoder) const
    {
        encoder << m_width;
        encoder << m_height;
        encoder << m_sampleRate;
        encoder << m_sampleSize;

        encoder << m_aspectRatio;
        encoder << m_frameRate;
        encoder << m_volume;

        encoder << m_echoCancellation;
        encoder << m_displaySurface;
        encoder << m_logicalSurface;

        encoder << m_facingMode;
        encoder << m_deviceId;
        encoder << m_groupId;
    }

    template <class Decoder> static Optional<MediaTrackConstraintSetMap> decode(Decoder& decoder)
    {
        MediaTrackConstraintSetMap map;
        if (!decoder.decode(map.m_width))
            return WTF::nullopt;
        if (!decoder.decode(map.m_height))
            return WTF::nullopt;
        if (!decoder.decode(map.m_sampleRate))
            return WTF::nullopt;
        if (!decoder.decode(map.m_sampleSize))
            return WTF::nullopt;

        if (!decoder.decode(map.m_aspectRatio))
            return WTF::nullopt;
        if (!decoder.decode(map.m_frameRate))
            return WTF::nullopt;
        if (!decoder.decode(map.m_volume))
            return WTF::nullopt;

        if (!decoder.decode(map.m_echoCancellation))
            return WTF::nullopt;
        if (!decoder.decode(map.m_displaySurface))
            return WTF::nullopt;
        if (!decoder.decode(map.m_logicalSurface))
            return WTF::nullopt;

        if (!decoder.decode(map.m_facingMode))
            return WTF::nullopt;
        if (!decoder.decode(map.m_deviceId))
            return WTF::nullopt;
        if (!decoder.decode(map.m_groupId))
            return WTF::nullopt;

        return WTFMove(map);
    }

private:
    Optional<IntConstraint> m_width;
    Optional<IntConstraint> m_height;
    Optional<IntConstraint> m_sampleRate;
    Optional<IntConstraint> m_sampleSize;

    Optional<DoubleConstraint> m_aspectRatio;
    Optional<DoubleConstraint> m_frameRate;
    Optional<DoubleConstraint> m_volume;

    Optional<BooleanConstraint> m_echoCancellation;
    Optional<BooleanConstraint> m_displaySurface;
    Optional<BooleanConstraint> m_logicalSurface;

    Optional<StringConstraint> m_facingMode;
    Optional<StringConstraint> m_deviceId;
    Optional<StringConstraint> m_groupId;
};

class FlattenedConstraint {
public:

    void set(const MediaConstraint&);
    void merge(const MediaConstraint&);
    void append(const MediaConstraint&);
    const MediaConstraint* find(MediaConstraintType) const;
    bool isEmpty() const { return m_variants.isEmpty(); }

    class iterator {
    public:
        iterator(const FlattenedConstraint* constraint, size_t index)
            : m_constraint(constraint)
            , m_index(index)
#ifndef NDEBUG
            , m_generation(constraint->m_generation)
#endif
        {
        }

        MediaConstraint& operator*() const
        {
            return m_constraint->m_variants.at(m_index).constraint();
        }

        iterator& operator++()
        {
#ifndef NDEBUG
            ASSERT(m_generation == m_constraint->m_generation);
#endif
            m_index++;
            return *this;
        }

        bool operator==(const iterator& other) const { return m_index == other.m_index; }
        bool operator!=(const iterator& other) const { return !(*this == other); }

    private:
        const FlattenedConstraint* m_constraint { nullptr };
        size_t m_index { 0 };
#ifndef NDEBUG
        int m_generation { 0 };
#endif
    };

    const iterator begin() const { return iterator(this, 0); }
    const iterator end() const { return iterator(this, m_variants.size()); }

private:
    class ConstraintHolder {
    public:
        static ConstraintHolder create(const MediaConstraint& value) { return ConstraintHolder(value); }

        ~ConstraintHolder()
        {
            if (m_value.asRaw) {
                switch (dataType()) {
                case MediaConstraint::DataType::Integer:
                    delete m_value.asInteger;
                    break;
                case MediaConstraint::DataType::Double:
                    delete m_value.asDouble;
                    break;
                case MediaConstraint::DataType::Boolean:
                    delete m_value.asBoolean;
                    break;
                case MediaConstraint::DataType::String:
                    delete m_value.asString;
                    break;
                case MediaConstraint::DataType::None:
                    ASSERT_NOT_REACHED();
                    break;
                }
            }
#ifndef NDEBUG
            m_value.asRaw = reinterpret_cast<MediaConstraint*>(0xbbadbeef);
#endif
        }

        ConstraintHolder(ConstraintHolder&& other)
        {
            switch (other.dataType()) {
            case MediaConstraint::DataType::Integer:
                m_value.asInteger = std::exchange(other.m_value.asInteger, nullptr);
                break;
            case MediaConstraint::DataType::Double:
                m_value.asDouble = std::exchange(other.m_value.asDouble, nullptr);
                break;
            case MediaConstraint::DataType::Boolean:
                m_value.asBoolean = std::exchange(other.m_value.asBoolean, nullptr);
                break;
            case MediaConstraint::DataType::String:
                m_value.asString = std::exchange(other.m_value.asString, nullptr);
                break;
            case MediaConstraint::DataType::None:
                ASSERT_NOT_REACHED();
                break;
            }
        }

        MediaConstraint& constraint() const { return *m_value.asRaw; }
        MediaConstraint::DataType dataType() const { return constraint().dataType(); }
        MediaConstraintType constraintType() const { return constraint().constraintType(); }

    private:
        explicit ConstraintHolder(const MediaConstraint& value)
        {
            switch (value.dataType()) {
            case MediaConstraint::DataType::Integer:
                m_value.asInteger = new IntConstraint(downcast<const IntConstraint>(value));
                break;
            case MediaConstraint::DataType::Double:
                m_value.asDouble = new DoubleConstraint(downcast<DoubleConstraint>(value));
                break;
            case MediaConstraint::DataType::Boolean:
                m_value.asBoolean = new BooleanConstraint(downcast<BooleanConstraint>(value));
                break;
            case MediaConstraint::DataType::String:
                m_value.asString = new StringConstraint(downcast<StringConstraint>(value));
                break;
            case MediaConstraint::DataType::None:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        
        union {
            MediaConstraint* asRaw;
            IntConstraint* asInteger;
            DoubleConstraint* asDouble;
            BooleanConstraint* asBoolean;
            StringConstraint* asString;
        } m_value;
    };

    Vector<ConstraintHolder> m_variants;
#ifndef NDEBUG
    int m_generation { 0 };
#endif
};

struct MediaConstraints {
    void setDefaultVideoConstraints();
    bool isConstraintSet(const WTF::Function<bool(const MediaTrackConstraintSetMap&)>&);

    MediaTrackConstraintSetMap mandatoryConstraints;
    Vector<MediaTrackConstraintSetMap> advancedConstraints;
    bool isValid { false };
};
    
} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(ConstraintType, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ConstraintType) \
static bool isType(const WebCore::MediaConstraint& constraint) { return constraint.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(IntConstraint, isInt())
SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(DoubleConstraint, isDouble())
SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(StringConstraint, isString())
SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(BooleanConstraint, isBoolean())

#endif // ENABLE(MEDIA_STREAM)
