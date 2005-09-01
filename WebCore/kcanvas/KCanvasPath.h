/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KCanvasPath_H
#define KCanvasPath_H

#include <qvaluelist.h>

class QTextStream;

typedef enum
{
    RULE_NONZERO = 0,
    RULE_EVENODD = 1
} KCWindRule;

QTextStream &operator<<(QTextStream &ts, KCWindRule rule);

// Path related data structures
typedef enum
{
    CMD_MOVE = 0,
    CMD_LINE = 1,
    CMD_CURVE = 2,
    CMD_CLOSE_SUBPATH = 3
} KCPathCommand;

QTextStream &operator<<(QTextStream &ts, KCPathCommand cmd);

struct KCPathData
{
    KCPathCommand cmd : 2;

    double x1, x2, x3;
    double y1, y2, y3;
};

QTextStream &operator<<(QTextStream &ts, const KCPathData &d);

class KCPathDataList : public QValueList<KCPathData>
{
public:
    KCPathDataList() { }

    inline void moveTo(double x, double y)
    {
        KCPathData data; data.cmd = CMD_MOVE;
        data.x3 = x; data.y3 = y;
        append(data);
    }

    inline void lineTo(double x, double y)
    {
        KCPathData data; data.cmd = CMD_LINE;
        data.x3 = x; data.y3 = y;
        append(data);
    }

    inline void curveTo(double x1, double y1, double x2, double y2, double x3, double y3)
    {
        KCPathData data; data.cmd = CMD_CURVE;
        data.x1 = x1; data.y1 = y1;
        data.x2 = x2; data.y2 = y2;
        data.x3 = x3; data.y3 = y3;
        append(data);
    }

    inline void closeSubpath()
    {
         KCPathData data; data.cmd = CMD_CLOSE_SUBPATH;
         append(data);
    }
};

// Clipping paths
struct KCClipData
{
    KCWindRule rule : 1;
    bool bbox : 1;
    bool viewportClipped : 1;
    KCPathDataList path;
};

QTextStream &operator<<(QTextStream &ts, const KCClipData &d);

class KCClipDataList : public QValueList<KCClipData>
{
public:
    KCClipDataList() { }

    inline void addPath(const KCPathDataList &pathData, KCWindRule rule, bool bbox, bool viewportClipped = false)
    {
        KCClipData data;
        data.rule = rule;
        data.bbox = bbox;
        data.path = pathData;
        data.viewportClipped = viewportClipped;
        append(data);
    }
};

#endif

// vim:ts=4:noet
