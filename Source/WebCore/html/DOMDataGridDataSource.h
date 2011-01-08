/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef DOMDataGridDataSource_h
#define DOMDataGridDataSource_h

#if ENABLE(DATAGRID)

#include "DataGridDataSource.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class HTMLDataGridElement;

class DOMDataGridDataSource : public DataGridDataSource {
public:
    static PassRefPtr<DOMDataGridDataSource> create()
    {
        return adoptRef(new DOMDataGridDataSource);
    }

    virtual ~DOMDataGridDataSource();

    virtual bool isDOMDataGridDataSource() const { return true; }

private:
    DOMDataGridDataSource();
};

inline DOMDataGridDataSource* asDOMDataGridDataSource(DataGridDataSource* dataSource)
{
    ASSERT(dataSource->isDOMDataGridDataSource());
    return static_cast<DOMDataGridDataSource*>(dataSource);
}

inline const DOMDataGridDataSource* asDOMDataGridDataSource(const DataGridDataSource* dataSource)
{
    ASSERT(dataSource->isDOMDataGridDataSource());
    return static_cast<const DOMDataGridDataSource*>(dataSource);
}

} // namespace WebCore

#endif // ENABLE(DATAGRID)
#endif // DOMDataGridDataSource_h
