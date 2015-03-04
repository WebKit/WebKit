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

#include "config.h"
#include "MicroTaskTest.h"

#include "Document.h"

namespace WebCore {

void MicroTaskTest::run()
{
    StringBuilder message;
    message.append("MicroTask #");
    message.append(String::number(m_testNumber));
    message.append(" has run.");
    if (m_document)
        m_document->addConsoleMessage(MessageSource::JS, MessageLevel::Debug, message.toString());
}

std::unique_ptr<MicroTaskTest> MicroTaskTest::create(WeakPtr<Document> document, int testNumber)
{
    return std::unique_ptr<MicroTaskTest>(new MicroTaskTest(document, testNumber));
}

} // namespace WebCore
