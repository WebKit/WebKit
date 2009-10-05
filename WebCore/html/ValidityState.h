/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ValidityState_h
#define ValidityState_h

#include "HTMLFormControlElement.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    class ValidityState : public RefCounted<ValidityState> {
    public:
        static PassRefPtr<ValidityState> create(HTMLFormControlElement* owner)
        {
            return adoptRef(new ValidityState(owner));
        }

        HTMLFormControlElement* control() const { return m_control; }

        void setCustomErrorMessage(const String& message) { m_customErrorMessage = message; }

        bool valueMissing() { return control()->valueMissing(); }
        bool typeMismatch();
        bool patternMismatch() { return control()->patternMismatch(); }
        bool tooLong() { return control()->tooLong(); }
        bool rangeUnderflow();
        bool rangeOverflow();
        bool stepMismatch() { return false; }
        bool customError() { return !m_customErrorMessage.isEmpty(); }
        bool valid();

    private:
        ValidityState(HTMLFormControlElement*);
        HTMLFormControlElement* m_control;
        String m_customErrorMessage;

        static bool isValidColorString(const String&);
    };

} // namespace WebCore

#endif // ValidityState_h
