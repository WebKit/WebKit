//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/Diagnostics.h"

#include "compiler/InfoSink.h"
#include "compiler/preprocessor/new/SourceLocation.h"

TDiagnostics::TDiagnostics(TInfoSink& infoSink) : mInfoSink(infoSink)
{
}

TDiagnostics::~TDiagnostics()
{
}

void TDiagnostics::writeInfo(Severity severity,
                             const pp::SourceLocation& loc,
                             const std::string& reason,
                             const std::string& token,
                             const std::string& extra)
{
    TInfoSinkBase& sink = mInfoSink.info;
    TPrefixType prefix = severity == ERROR ? EPrefixError : EPrefixWarning;

    /* VC++ format: file(linenum) : error #: 'token' : extrainfo */
    sink.prefix(prefix);
    sink.location(EncodeSourceLoc(loc.file, loc.line));
    sink << "'" << token <<  "' : " << reason << " " << extra << "\n";
}

void TDiagnostics::writeDebug(const std::string& str)
{
    mInfoSink.debug << str;
}

void TDiagnostics::print(ID id,
                         const pp::SourceLocation& loc,
                         const std::string& text)
{
}
