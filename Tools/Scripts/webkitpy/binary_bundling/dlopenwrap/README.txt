This is a shared library to wrap dlopen() and dlmopen() function calls
and rewrite the paths loaded to ensure that the library loaded is inside
LD_LIBRARY_PATH environment variables.

It is used by the script generate-bundle that builds this library and adds
it to be preloaded into the wrapper script that executes the browser.

Set the environment variable DLOPEN_WRAP_DEBUG=1 or DLOPEN_WRAP_DEBUG=2
to get error/debug output from this library.
