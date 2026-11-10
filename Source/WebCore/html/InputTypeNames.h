/*
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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

#pragma once

#include <wtf/text/AtomString.h>

namespace WebCore {

namespace InputTypeNames {

NODELETE const RefPtr<AtomStringImpl>& button();
const RefPtr<AtomStringImpl>& checkbox();
const RefPtr<AtomStringImpl>& color();
const RefPtr<AtomStringImpl>& date();
const RefPtr<AtomStringImpl>& datetimelocal();
NODELETE const RefPtr<AtomStringImpl>& email();
const RefPtr<AtomStringImpl>& file();
const RefPtr<AtomStringImpl>& hidden();
const RefPtr<AtomStringImpl>& image();
const RefPtr<AtomStringImpl>& month();
const RefPtr<AtomStringImpl>& number();
const RefPtr<AtomStringImpl>& password();
const RefPtr<AtomStringImpl>& radio();
const RefPtr<AtomStringImpl>& range();
NODELETE const RefPtr<AtomStringImpl>& reset();
NODELETE const RefPtr<AtomStringImpl>& search();
NODELETE const RefPtr<AtomStringImpl>& submit();
NODELETE const RefPtr<AtomStringImpl>& telephone();
NODELETE const RefPtr<AtomStringImpl>& text();
const RefPtr<AtomStringImpl>& time();
NODELETE const RefPtr<AtomStringImpl>& url();
const RefPtr<AtomStringImpl>& week();

}

} // namespace WebCore
