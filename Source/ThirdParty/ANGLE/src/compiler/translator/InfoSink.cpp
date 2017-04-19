//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/InfoSink.h"

namespace sh
{

void TInfoSinkBase::prefix(Severity severity)
{
    switch (severity)
    {
        case SH_WARNING:
            sink.append("WARNING: ");
            break;
        case SH_ERROR:
            sink.append("ERROR: ");
            break;
        default:
            sink.append("UNKOWN ERROR: ");
            break;
    }
}

void TInfoSinkBase::location(int file, int line)
{
    TPersistStringStream stream;
    if (line)
        stream << file << ":" << line;
    else
        stream << file << ":? ";
    stream << ": ";

    sink.append(stream.str());
}

}  // namespace sh
