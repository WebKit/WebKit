#ifndef WebKitAccessibleInterfaceTableCell_h
#define WebKitAccessibleInterfaceTableCell_h

#if HAVE(ACCESSIBILITY)

#include <atk/atk.h>

#if ATK_CHECK_VERSION(2,11,90)
void webkitAccessibleTableCellInterfaceInit(AtkTableCellIface*);
#endif

#endif // HAVE(ACCESSIBILITY)
#endif // WebKitAccessibleInterfaceTableCell_h
