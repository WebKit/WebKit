
#include "libyuv/convert.h"

#include <stdio.h>   // for printf
#include <string.h>  // for memset

int main(int, char**) {
  unsigned char src_i444[640 * 400 * 3];
  unsigned char dst_nv12[640 * 400 * 3 / 2];

  for (size_t i = 0; i < sizeof(src_i444); ++i) {
    src_i444[i] = i & 255;
  }
  memset(dst_nv12, 0, sizeof(dst_nv12));
  libyuv::I444ToNV12(&src_i444[0], 640,              // source Y
                     &src_i444[640 * 400], 640,      // source U
                     &src_i444[640 * 400 * 2], 640,  // source V
                     &dst_nv12[0], 640,              // dest Y
                     &dst_nv12[640 * 400], 640,      // dest UV
                     640, 400);                      // width and height

  int checksum = 0;
  for (size_t i = 0; i < sizeof(dst_nv12); ++i) {
    checksum += dst_nv12[i];
  }
  printf("checksum %x %s\n", checksum, checksum == 0x2ec0c00 ? "PASS" : "FAIL");
  return 0;
}