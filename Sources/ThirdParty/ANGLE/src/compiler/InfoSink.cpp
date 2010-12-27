//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/InfoSink.h"

void TInfoSinkBase::prefix(TPrefixType message) {
    switch(message) {
        case EPrefixNone:
            break;
        case EPrefixWarning:
            sink.append("WARNING: ");
            break;
        case EPrefixError:
            sink.append("ERROR: ");
            break;
        case EPrefixInternalError:
            sink.append("INTERNAL ERROR: ");
            break;
        case EPrefixUnimplemented:
            sink.append("UNIMPLEMENTED: ");
            break;
        case EPrefixNote:
            sink.append("NOTE: ");
            break;
        default:
            sink.append("UNKOWN ERROR: ");
            break;
    }
}

void TInfoSinkBase::location(TSourceLoc loc) {
    int string = loc >> SourceLocStringShift;
    int line = loc & SourceLocLineMask;

    TPersistStringStream stream;
    if (line)
        stream << string << ":" << line;
    else
        stream << string << ":? ";
    stream << ": ";

    sink.append(stream.str());
}

void TInfoSinkBase::message(TPrefixType message, const char* s) {
    prefix(message);
    sink.append(s);
    sink.append("\n");
}

void TInfoSinkBase::message(TPrefixType message, const char* s, TSourceLoc loc) {
    prefix(message);
    location(loc);
    sink.append(s);
    sink.append("\n");
}
