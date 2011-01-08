This directory is intended to contain copies of third-party libraries used
by WebCore, in particular those which may require some modification in
order to incorporate.

The current convention is to fully scope the include paths of headers, in
order to avoid collisions with other WebCore headers. For example, a file
in WebCore using the GLU tessellator sources would use

#include "thirdparty/glu/internal_glu.h"

Header references in source files in this directory need to be similarly
modified.
