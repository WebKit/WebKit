//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "OSWindow.h"

OSWindow::OSWindow()
    : mX(0),
      mY(0),
      mWidth(0),
      mHeight(0)
{
}

OSWindow::~OSWindow()
{}

int OSWindow::getX() const
{
    return mX;
}

int OSWindow::getY() const
{
    return mY;
}

int OSWindow::getWidth() const
{
    return mWidth;
}

int OSWindow::getHeight() const
{
    return mHeight;
}

bool OSWindow::popEvent(Event *event)
{
    if (mEvents.size() > 0 && event)
    {
        *event = mEvents.front();
        mEvents.pop_front();
        return true;
    }
    else
    {
        return false;
    }
}

void OSWindow::pushEvent(Event event)
{
    switch (event.Type)
    {
      case Event::EVENT_MOVED:
        mX = event.Move.X;
        mY = event.Move.Y;
        break;
      case Event::EVENT_RESIZED:
        mWidth = event.Size.Width;
        mHeight = event.Size.Height;
        break;
      default:
        break;
    }

    mEvents.push_back(event);
}

bool OSWindow::didTestEventFire()
{
    Event topEvent;
    while (popEvent(&topEvent))
    {
        if (topEvent.Type == Event::EVENT_TEST)
        {
            return true;
        }
    }

    return false;
}
