/* This file is part of the KDE project
 *
 * Copyright (C) 2000 Simon Hausmann <hausmann@kde.org>
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
 */
#ifndef __khtml_find_h__
#define __khtml_find_h__

#include <keditcl.h>

class KHTMLPart;

class KHTMLFind : public KEdFind
{
  Q_OBJECT
public:
  KHTMLFind( KHTMLPart *part, QWidget *parent, const char *name );
  virtual ~KHTMLFind();

  KHTMLPart *part() const { return m_part; }
  void setPart( KHTMLPart *part ) { m_part = part; setNewSearch(); }
  void setNewSearch();

private slots:
  void slotDone();
  void slotSearch();

private:
  bool m_first;
  bool m_found;
  KHTMLPart *m_part;
};

#endif
