

#include <CoreFoundation/CoreFoundation.h>


static void
usage(const char *program)
{
  printf("Usage: %s OUTFILE\n", program);
  exit(1);
}

int
main (int argc, char **argv)
{
  const CFStringEncoding *all_encodings;
  const CFStringEncoding *p;
  CFStringRef name;
  char cname[2048];
  FILE *output;

  if (argc != 2) {
    usage(argv[0]);
  }
  
  output = fopen (argv[1], "w");

  if (output == NULL) {
    printf ("Cannot open file `%s'\n", argv[1]);
  }

  all_encodings = CFStringGetListOfAvailableEncodings();

  for (p = all_encodings; *p != kCFStringEncodingInvalidId; p++) {
    name = CFStringConvertEncodingToIANACharSetName(*p);
    /* All IANA encoding names must be US-ASCII */
    if (name != NULL) {
      CFStringGetCString(name, cname, 2048, kCFStringEncodingASCII);
      fprintf(output,"%ld:%s\n", *p, cname);
    } else {
      printf("Warning: nameless encoding %ld\n", *p);
    }
  }
  return 0;
}
