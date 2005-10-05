/*
 * Copyright (C) 2005 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "KCanvasPath.h"
#include <qtextstream.h>
#include "KCanvasTreeDebug.h"

//KCPathCommand
QTextStream &operator<<(QTextStream &ts, KCPathCommand cmd)
{
    switch (cmd) 
    {
        case CMD_MOVE:
            ts << "MOVE"; break;
        case CMD_LINE:
            ts << "LINE"; break;
        case CMD_CURVE:
            ts << "CURVE"; break;
        case CMD_CLOSE_SUBPATH:
            ts << "CLOSE_SUBPATH"; break;
    }
    return ts;
}

//KCWindRule
QTextStream &operator<<(QTextStream &ts, KCWindRule rule)
{
    switch (rule) 
    {
        case RULE_NONZERO:
            ts << "NON-ZERO"; break;
        case RULE_EVENODD:
            ts << "EVEN-ODD"; break;
    }
    return ts;
}

//KCPathData
QTextStream &operator<<(QTextStream &ts, const KCPathData &d)
{
    ts << "{cmd=" << d.cmd;
    switch(d.cmd)
    {    
        case CMD_CURVE:  
            ts << " x1="<< d.x1 << " y1=" << d.y1
                << " x2="<< d.x2 << " y2=" << d.y2;
            /* fallthrough */
        case CMD_LINE:
        case CMD_MOVE:
            ts << " x3="<< d.x3 << " y3=" << d.y3;
            break;
        case CMD_CLOSE_SUBPATH:
            /*empty*/
            break;
    }
    ts << "}";
    return ts;
}

//KCClipData
QTextStream &operator<<(QTextStream &ts, const KCClipData &d)
{
    ts << "[winding=" << d.rule  << "]";
    if (d.bbox)
        ts << " [bounding box mode=" << d.bbox  << "]";
    if (d.viewportClipped)
        ts << " [viewport clipped=" << d.viewportClipped << "]";      
    ts << " [path=" << d.path << "]";
    return ts;
}
