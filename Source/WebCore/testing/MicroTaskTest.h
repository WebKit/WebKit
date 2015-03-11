/*
 * Copyright (C) 2015 Akamai Technologies Inc. All rights reserved.
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

#ifndef MicroTaskTest_h
#define MicroTaskTest_h

#include "MicroTask.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;

class MicroTaskTest : public MicroTask {
public:
    MicroTaskTest(WeakPtr<Document> document, int testNumber)
        : m_document(document)
        , m_testNumber(testNumber)
    {
    }

    virtual void run() override;
    WEBCORE_TESTSUPPORT_EXPORT static std::unique_ptr<MicroTaskTest> create(WeakPtr<Document>, int testNumber);

private:
    WeakPtr<Document> m_document;
    int m_testNumber;
};

} // namespace WebCore

#endif
