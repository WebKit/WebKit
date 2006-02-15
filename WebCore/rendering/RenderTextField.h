/*
 * Copyright (C) 2006 Apple Computer
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef RenderTextField_H
#define RenderTextField_H

#include "dom2_events.h"
#include "render_flexbox.h"
#include "html_blockimpl.h"

namespace WebCore
{

class RenderTextField;

class InputMutationListener : public EventListener
{
public:
    InputMutationListener() { m_renderTextField = 0; }
    InputMutationListener(RenderTextField *r) { m_renderTextField = r; }
    RenderTextField *renderTextField() const { return m_renderTextField; }
    void setInputElement(RenderTextField *r) { m_renderTextField = r; }
    
    virtual void handleEvent(EventListenerEvent evt, bool isWindowEvent);
    
private:
    RenderTextField *m_renderTextField;
};

class RenderTextField : public RenderBlock
{
public:
    RenderTextField(NodeImpl* node);
    virtual ~RenderTextField();

    virtual void calcMinMaxWidth();
    virtual const char *renderName() const { return "RenderTextField"; }
    virtual void removeLeftoverAnonymousBoxes() {};
    virtual void setStyle(RenderStyle* style);
    virtual void updateFromElement();
    
    RenderStyle* createDivStyle(RenderStyle* startStyle);

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);    
    void select();
    void setSelectionRange(int, int);

    void subtreeHasChanged();

protected:
    RefPtr<HTMLDivElementImpl> m_div;
    RefPtr<InputMutationListener> m_mutationListener;
};

};
#endif
