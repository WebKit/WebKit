#include <stdio.h>
#include <string.h>
#include <pcre.h>

/* Compile thuswise:
  gcc -Wall pcredemo.c -I/opt/local/include -L/opt/local/lib \
    -R/opt/local/lib -lpcre
*/

#define OVECCOUNT 30    /* should be a multiple of 3 */

int main(int argc, char **argv)
{
pcre *re;
const char *error;
int erroffset;
int ovector[OVECCOUNT];
int rc, i;

if (argc != 3)
  {
  printf("Two arguments required: a regex and a subject string\n");
  return 1;
  }

/* Compile the regular expression in the first argument */

re = pcre_compile(
  argv[1],              /* the pattern */
  0,                    /* default options */
  &error,               /* for error message */
  &erroffset,           /* for error offset */
  NULL);                /* use default character tables */

/* Compilation failed: print the error message and exit */

if (re == NULL)
  {
  printf("PCRE compilation failed at offset %d: %s\n", erroffset, error);
  return 1;
  }

/* Compilation succeeded: match the subject in the second argument */

rc = pcre_exec(
  re,                   /* the compiled pattern */
  NULL,                 /* no extra data - we didn't study the pattern */
  argv[2],              /* the subject string */
  (int)strlen(argv[2]), /* the length of the subject */
  0,                    /* start at offset 0 in the subject */
  0,                    /* default options */
  ovector,              /* output vector for substring information */
  OVECCOUNT);           /* number of elements in the output vector */

/* Matching failed: handle error cases */

if (rc < 0)
  {
  switch(rc)
    {
    case PCRE_ERROR_NOMATCH: printf("No match\n"); break;
    /*
    Handle other special cases if you like
    */
    default: printf("Matching error %d\n", rc); break;
    }
  return 1;
  }

/* Match succeded */

printf("Match succeeded\n");

/* The output vector wasn't big enough */

if (rc == 0)
  {
  rc = OVECCOUNT/3;
  printf("ovector only has room for %d captured substrings\n", rc - 1);
  }

/* Show substrings stored in the output vector */

for (i = 0; i < rc; i++)
  {
  char *substring_start = argv[2] + ovector[2*i];
  int substring_length = ovector[2*i+1] - ovector[2*i];
  printf("%2d: %.*s\n", i, substring_length, substring_start);
  }

return 0;
}


