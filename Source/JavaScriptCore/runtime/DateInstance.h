/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/PlainGregorianDateTime.h>

namespace JSC {

class DateInstance final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.dateInstanceSpace();
    }

    static DateInstance* create(VM& vm, Structure* structure, double date)
    {
        DateInstance* instance = new (NotNull, allocateCell<DateInstance>(vm)) DateInstance(vm, structure);
        instance->finishCreation(vm, date);
        return instance;
    }

    static DateInstance* create(VM& vm, Structure* structure)
    {
        DateInstance* instance = new (NotNull, allocateCell<DateInstance>(vm)) DateInstance(vm, structure);
        instance->finishCreation(vm);
        return instance;
    }

    double internalNumber() const { return m_internalNumber; }
    void setInternalNumber(double value)
    {
        // Marked stale rather than cleared: a cleared payload means this instance has never
        // decomposed anything, which is what tells DateCache its shared memo is worth probing.
        m_internalNumber = value;
        m_cachedGregorianDateTime = PlainGregorianDateTime::staleMarker();
        m_cachedGregorianDateTimeUTC = PlainGregorianDateTime::staleMarker();
    }

    DECLARE_EXPORT_INFO;

    PlainGregorianDateTime gregorianDateTime(DateCache& cache) const
    {
        if (m_cachedGregorianDateTime)
            return m_cachedGregorianDateTime;
        return calculateGregorianDateTime(cache);
    }

    PlainGregorianDateTime gregorianDateTimeUTC(DateCache& cache) const
    {
        if (m_cachedGregorianDateTimeUTC)
            return m_cachedGregorianDateTimeUTC;
        return calculateGregorianDateTimeUTC(cache);
    }

    // A cached local-time breakdown is only valid for the time zone it was computed in, and
    // nothing tracks which instances hold one, so every live Date is swept on a zone change.
    // Cleared rather than marked stale: DateCache has just dropped its shared memo too, so the
    // next decomposition may as well repopulate it.
    void invalidateCachedLocalGregorianDateTime() { m_cachedGregorianDateTime = { }; }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static constexpr ptrdiff_t offsetOfInternalNumber() { return OBJECT_OFFSETOF(DateInstance, m_internalNumber); }
    static constexpr ptrdiff_t offsetOfCachedGregorianDateTime() { return OBJECT_OFFSETOF(DateInstance, m_cachedGregorianDateTime); }
    static constexpr ptrdiff_t offsetOfCachedGregorianDateTimeUTC() { return OBJECT_OFFSETOF(DateInstance, m_cachedGregorianDateTimeUTC); }

private:
    JS_EXPORT_PRIVATE DateInstance(VM&, Structure*);

    DECLARE_DEFAULT_FINISH_CREATION;
    JS_EXPORT_PRIVATE void finishCreation(VM&, double);
    JS_EXPORT_PRIVATE PlainGregorianDateTime calculateGregorianDateTime(DateCache&) const;
    JS_EXPORT_PRIVATE PlainGregorianDateTime calculateGregorianDateTimeUTC(DateCache&) const;

    double m_internalNumber { PNaN };
    mutable PlainGregorianDateTime m_cachedGregorianDateTime;
    mutable PlainGregorianDateTime m_cachedGregorianDateTimeUTC;
};

} // namespace JSC
