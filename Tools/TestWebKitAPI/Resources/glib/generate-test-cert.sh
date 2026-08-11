#!/bin/sh

certtool --generate-self-signed --load-privkey test-key.pem  \
   --template test-cert.conf --outfile test-cert.pem
