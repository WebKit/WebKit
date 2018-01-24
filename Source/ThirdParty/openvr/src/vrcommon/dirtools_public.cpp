//========= Copyright Valve Corporation ============//
#include "dirtools_public.h"
#include "strtools_public.h"
#include "pathtools_public.h"

#include <errno.h>
#include <string.h>

#ifdef _WIN32
#include "windows.h"
#else
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined( OSX )
#include <sys/syslimits.h>
#endif


//-----------------------------------------------------------------------------
// Purpose: utility function to create dirs & subdirs
//-----------------------------------------------------------------------------
bool BCreateDirectoryRecursive( const char *pchPath )
{
       // Does it already exist?
       if ( Path_IsDirectory( pchPath ) )
              return true;

       // copy the path into something we can munge
       int len = (int)strlen( pchPath );
       char *path = (char *)malloc( len + 1 );
       strcpy( path, pchPath );

       // Walk backwards to first non-existing dir that we find
       char *s = path + len - 1;

       const char slash = Path_GetSlash();
       while ( s > path )
       {
              if ( *s == slash )
              {
                     *s = '\0';
                     bool bExists = Path_IsDirectory( path );
                     *s = slash;

                     if ( bExists )
                     {
                            ++s;
                            break;
                     }
              }
              --s;
       }

       // and then move forwards from there

       while ( *s )
       {
              if ( *s == slash )
              {
                     *s = '\0';
                     BCreateDirectory( path );
                     *s = slash;
              }
              s++;
       }

       bool bRetVal = BCreateDirectory( path );
       free( path );
       return bRetVal;
}


//-----------------------------------------------------------------------------
// Purpose: Creates the directory, returning true if it is created, or if it already existed
//-----------------------------------------------------------------------------
bool BCreateDirectory( const char *pchPath )
{
#ifdef WIN32
       std::wstring wPath = UTF8to16( pchPath );
       if ( ::CreateDirectoryW( wPath.c_str(), NULL ) )
              return true;

       if ( ::GetLastError() == ERROR_ALREADY_EXISTS )
              return true;

       return false;
#else
       int i = mkdir( pchPath, S_IRWXU | S_IRWXG | S_IRWXO );
       if ( i == 0 )
              return true;
       if ( errno == EEXIST )
              return true;

       return false;
#endif
}

