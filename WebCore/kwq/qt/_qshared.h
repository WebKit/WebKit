/***********************************************************************
 *
 * This file contains temporary implementations of classes from QT/KDE
 *
 * This code is mostly borrowed from QT/KDE and is only modified
 * so it builds and works with KWQ.
 *
 * Eventually we will need to either replace this file, or arrive at
 * some other solution that will enable us to ship this code.
 *
 ***********************************************************************/

#ifndef _QSHARED_H_
#define _QSHARED_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// KWQ hacks ---------------------------------------------------------------

#include <KWQDef.h>

// -------------------------------------------------------------------------

struct QShared
{
    QShared() 
    {
        count = 1;
    }

    void ref()
    {
        count++;
    }

    bool deref()
    {
        count--;
        return !count;
    }   

    uint count;
};

#endif
