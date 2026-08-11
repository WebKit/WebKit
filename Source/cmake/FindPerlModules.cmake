#.rst:
# FindPerlModules
# ---------------
#
# Check that Perl has certain modules available.
#
# If PERL_EXECUTABLE is set, uses that, otherwise calls the Perl find module.
#
# To use, pass the perl module names (in the form you would use in a Perl
# ``use`` statement) as components.
#
# This will define the following variables:
#
# ``Perl_<module>_FOUND``
#     True if the given Perl module could be loaded by Perl
#
# where ``<module>`` is either the name passed as a component, or a version
# with ``::`` replaced by ``_``.

# Copyright 2015 Alex Merry <alex.merry@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

if (NOT PERL_EXECUTABLE)
    find_package(Perl)
endif ()

include(FindPackageHandleStandardArgs)

if (PERL_EXECUTABLE)
    set(PerlModules_all_modules_found TRUE)
    foreach (_comp ${PerlModules_FIND_COMPONENTS})
        execute_process(
            COMMAND ${PERL_EXECUTABLE} -e "use ${_comp}"
            RESULT_VARIABLE _result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if (_result EQUAL 0)
            set(PerlModules_${_comp}_FOUND TRUE)
        else ()
            set(PerlModules_${_comp}_FOUND FALSE)
            set(PerlModules_all_modules_found FALSE)
        endif ()
    endforeach ()
endif ()

find_package_handle_standard_args(PerlModules
    FOUND_VAR
        PerlModules_FOUND
    REQUIRED_VARS
        PerlModules_all_modules_found
    HANDLE_COMPONENTS
)

include(FeatureSummary)
set_package_properties(PerlModules PROPERTIES
    URL "http://www.cpan.org"
)
