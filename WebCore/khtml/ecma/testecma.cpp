/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * An interactive interpreter to test the ECMA Script language bindings
 * for the DOM of KHTML.
 * The 'document' property is preset to an instance of Document and serves
 * as an entrypoint.
 *
 * Example session:
 *
 *   KJS> text = document.createTextNode('foo');
 *   KJS> document.appendChild(text);
 *   KJS> debug(document.firstChild.nodeValue);
 *   ---> foo
 */

#include <stdio.h>
#include <kjs/kjs.h>
#include <dom_doc.h>
#include "kjs_dom.h"

#include <dom_string.h>

using namespace KJS;

int main(int, char **)
{
  KJScript kjs;
  kjs.enableDebug();
  DOM::Document doc;

  DOMDocument *dd = new DOMDocument(&doc);
  Global::current().put("document", KJSO(dd));

  printf("Entering interactive mode.\n"
	 "You may access the DOM via the 'document' property.\n"
	 "Use debug() to print to the console. Press C-d or C-c to exit.\n\n");

  char buffer[1000];
  FILE *in = fdopen(0, "r");

  while (1) {
    printf("KJS> ");
    if (!fgets(buffer, 999, in))
      break;
    kjs.evaluate(buffer);
  }
  printf("\n");
}
