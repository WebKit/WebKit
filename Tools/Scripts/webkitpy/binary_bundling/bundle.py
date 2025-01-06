# Copyright (C) 2018, 2020, 2021, 2024 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import logging
import os
import shutil
import subprocess

_log = logging.getLogger(__name__)


WRAPPER_CSOURCE_TEMPLATE = """\
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char* resolve_abs_path(char *arg) {
    char *abs_path = realpath(arg, NULL);
    if (abs_path)
        return abs_path;
    return arg;
}

void args_update_relpaths_to_abs(int argc, char **argv) {
    for (int i = 0; i < argc; i++)
        if (argv[i][0] != '/' && !access(argv[i], F_OK))
            argv[i] = resolve_abs_path(argv[i]);
}

int file_grep(const char *filename, const char *search_str) {

    if (access(filename, F_OK))
        return 0;

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return 0;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int found = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        if (strstr(line, search_str)) {
            found = 1;
            break;
        }
    }

    free(line);
    fclose(file);

    return found;
}

void set_env_with_mydir(const char *env_var, const char *relative_path, const char *mydir) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%%s/%%s", mydir, relative_path);
    setenv(env_var, path, 1);
}


void set_ca_file(const char *mydir) {
    const char *system_ca_files[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/ssl/cert.pem",
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
    };

    for (int i = 0; i < sizeof(system_ca_files) / sizeof(system_ca_files[0]); i++) {
        if (file_grep(system_ca_files[i], "BEGIN CERTIFICATE")) {
            setenv("WEBKIT_TLS_CAFILE_PEM", system_ca_files[i], 1);
            return;
        }
    }

    // if we can't find a valid system cert use the one we bundle.
    set_env_with_mydir("WEBKIT_TLS_CAFILE_PEM", "sys/share/certs/bundle-ca-certificates.pem", mydir);
}

int exec_and_wait(const char *program, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
    if (pid == 0) { //child
        execvp(program, argv);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {  //parent
        int status;
        pid_t wpid = waitpid(pid, &status, 0);
        if (wpid == -1) {
            perror("waitpid");
            return -1;
        }
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
    }
}

int maybe_update_gdx_pixbuf_cache(const char *mydir) {
    char gdk_pixbuf_module_file[PATH_MAX];
    char gdk_pixbuf_module_dir[PATH_MAX];
    snprintf(gdk_pixbuf_module_file, sizeof(gdk_pixbuf_module_file), "%%s/sys/lib/gdk-pixbuf/loaders.cache", mydir);
    snprintf(gdk_pixbuf_module_dir, sizeof(gdk_pixbuf_module_dir), "%%s/sys/lib/gdk-pixbuf/loaders", mydir);

    if (file_grep(gdk_pixbuf_module_file, gdk_pixbuf_module_dir))
        return 0;

    char *gdk_pixbuf_args[] = { "gdk-pixbuf-query-loaders", "--update-cache", NULL };
    return exec_and_wait("bin/gdk-pixbuf-query-loaders", gdk_pixbuf_args);
}

int main(int argc, char *argv[]) {
    // options
    int should_set_ca_file = %(should_set_ca_file_value)s;
    int should_xkb_bundle  = %(should_xkb_bundle_value)s;
    int should_update_gdx_pixbuf_cache  = %(should_update_gdx_pixbuf_cache_value)s;


    // Get the directory of the executable
    char mydir[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", mydir, sizeof(mydir) - 1);
    if (len == -1) {
        perror("readlink");
        exit(EXIT_FAILURE);
    }
    mydir[len] = '\\0';
    strcpy(mydir, dirname(mydir));

    // Update args with full paths
    args_update_relpaths_to_abs(argc, argv);

    // Change to the directory where the script resides
    if (chdir(mydir) == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    if (should_set_ca_file)
        set_ca_file(mydir);

    %(export_env_variables)s

    if (should_xkb_bundle && !access("/usr/share/X11/xkb", F_OK))
        set_env_with_mydir("XKB_CONFIG_ROOT", "sys/share/xkb", mydir);

    char ldpath[PATH_MAX];
    %(set_ld_path_value)s
    setenv("LD_LIBRARY_PATH", ldpath, 1);

    if (should_update_gdx_pixbuf_cache) {
        int retcode = maybe_update_gdx_pixbuf_cache(mydir);
        if (retcode)
            fprintf(stderr, "gdk-pixbuf cache update command returned non-zero: %%d\\n", retcode);
    }

    char realProgram[] = "%(relative_path_binary)s";
    execv(realProgram, argv);

    // execv failed
    perror("execv");
    exit(EXIT_FAILURE);
}

"""


class BinaryBundler():
    VAR_MYDIR = 'MYDIR'

    def __init__(self, destination_dir, build_path=None):
        self._destination_dir = destination_dir
        self._has_patched_interpreter_relpath = False
        self._build_path = build_path
        self._should_use_sys_lib_directory = False

    def _run_cmd_and_get_output(self, command):
        _log.debug("EXEC %s" % command)
        command_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf-8')
        stdout, stderr = command_process.communicate()
        return command_process.returncode, stdout, stderr

    def _run_cmd_or_log_fail(self, command, failure_fatal=True):
        _log.debug("EXEC %s" % command)
        command_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf-8')
        stdout, stderr = command_process.communicate()
        if command_process.returncode != 0:
            failure_msg = "The command \"%s\" returned non-zero status: %d.\n\tstdout: %s\n\tstderr: %s" % (" ".join(command), command_process.returncode, stdout, stderr)
            if failure_fatal:
                raise RuntimeError(failure_msg)
            _log.error(failure_msg)

    def _is_system_dep(self, path):
        if self._build_path is not None:
            # We don't consider libwpe and libWPEBackend-fdo syslibs (even when those are not built directly). Neither dlopenwrap
            if any(p in path.lower() for p in ['libwpe', 'dlopenwrap']):
                return False
            return not path.startswith(self._build_path)
        return False

    def set_use_sys_lib_directory(self, should_use_sys_lib_directory):
        self._should_use_sys_lib_directory = should_use_sys_lib_directory

    def copy(self, orig_file, subdir=None):
        destination_dir = self._destination_dir
        if subdir is not None:
            assert(not os.path.isabs(subdir))
            destination_dir = os.path.join(destination_dir, subdir)
        if not os.path.isdir(destination_dir):
            os.makedirs(destination_dir)
        shutil.copy(orig_file, destination_dir)

    def copy_and_maybe_strip_patchelf(self, orig_file, type='bin', strip=True, patchelf_removerpath=True, patchelf_nodefaultlib=False, patchelf_setinterpreter_relativepath=None, object_final_destination_dir=None):
        """ This does the following:
            1. Copies the binary/lib (object)
            2. Optionally runs patchelf with different parameters
            3. Optionally strips the binary
            If the file is a symlink pointing another file then the symlink is preserved (copying the file pointed)
        """

        dir_suffix = 'lib' if type == 'interpreter' else type
        if object_final_destination_dir is None:
            object_final_destination_dir = self._destination_dir
            # If set_use_sys_lib_directory() is enabled then system libraries are shipped in $ROOT/sys/lib to allow to redistribute them separately.
            # However, the interpreter is always shipped in $ROOT/lib and all the binaries are always shipped in $ROOT/bin
            # Even when they are system_deps because of patchelf_setinterpreter_relativepath
            if self._should_use_sys_lib_directory and self._is_system_dep(orig_file) and type not in ['interpreter', 'bin']:
                object_final_destination_dir = os.path.join(object_final_destination_dir, 'sys')
            object_final_destination_dir = os.path.join(object_final_destination_dir, dir_suffix)
        if not os.path.isdir(object_final_destination_dir):
            os.makedirs(object_final_destination_dir)

        if not os.path.isfile(orig_file):
            raise ValueError('Can not find file %s' % orig_file)

        _log.info('Add to bundle [%s]: %s' % (type, orig_file))

        # Preserve symlinks and copy the targets
        if os.path.islink(orig_file):
            symlink_src_file = os.path.realpath(orig_file)
            symlink_src_basename = os.path.basename(symlink_src_file)
            symlink_dst_basename = os.path.basename(orig_file)
            symlink_dst_fullpath = os.path.join(object_final_destination_dir, symlink_dst_basename)
            if symlink_src_basename != symlink_dst_basename:
                try:
                    # symlink_dst_fullpath -> symlink_src_basename
                    os.symlink(symlink_src_basename, symlink_dst_fullpath)
                except FileExistsError:
                    previous_symlink_src_path = os.path.realpath(symlink_dst_fullpath)
                    previous_symlink_src_basename = os.path.basename(previous_symlink_src_path)
                    if previous_symlink_src_basename != symlink_src_basename:
                        raise RuntimeError('Not overwriting previous symlink %s pointing to %s with a symlink pointing to %s' % (symlink_dst_fullpath, previous_symlink_src_basename, symlink_src_basename))
                return self.copy_and_maybe_strip_patchelf(symlink_src_file, type, strip, patchelf_removerpath, patchelf_nodefaultlib, patchelf_setinterpreter_relativepath, object_final_destination_dir)

        try:
            shutil.copy(orig_file, object_final_destination_dir)
        except shutil.SameFileError:
            # May reasonably happen if the caller tries to bundle the files 'in place'.
            pass

        if type == 'interpreter':
            return  # no strip/patchelf over it

        # Running 'strip' after 'patchelf' may corrupt binaries, so run first 'strip'.
        # See: https://github.com/NixOS/patchelf/issues/371
        if strip:
            if not shutil.which('strip'):
                _log.warning('Unable to find the strip command in the system. Please install it.')
                raise Exception('Missing required program "strip"')
            strip_command = ['strip', '--strip-unneeded', os.path.join(object_final_destination_dir, os.path.basename(orig_file))]
            self._run_cmd_or_log_fail(strip_command, failure_fatal=False)

        patchelf_arguments = []
        if patchelf_removerpath:
            patchelf_arguments.append("--remove-rpath")
        if type == 'bin':
            if patchelf_nodefaultlib:
                patchelf_arguments.append("--no-default-lib")
            if patchelf_setinterpreter_relativepath:
                previous_cwd = os.getcwd()
                os.chdir(self.destination_dir())
                # We only want the basename of the interpreter because we are updating it
                # to use a relative path inside lib/ (which is were we copied it previously)
                new_interpreter_relpath = os.path.join('lib', os.path.basename(patchelf_setinterpreter_relativepath))
                if not os.path.isfile(new_interpreter_relpath):
                    raise ValueError('Unable to find interpreter %s at directory %s' % (new_interpreter_relpath, self.destination_dir()))
                patchelf_arguments.extend(['--set-interpreter', new_interpreter_relpath])
                self._has_patched_interpreter_relpath = True
        if patchelf_arguments:
            # If we have resolved patchelf_arguments then we should run patchelf
            patchelf_arguments.append(os.path.join(object_final_destination_dir, os.path.basename(orig_file)))
            if not shutil.which('patchelf'):
                raise RuntimeError('Unable to find the "patchelf" command in the system. Please install it.')
            self._run_cmd_or_log_fail(['patchelf'] + patchelf_arguments)

            if patchelf_setinterpreter_relativepath:
                os.chdir(previous_cwd)

    def destination_dir(self):
        return self._destination_dir

    def is_xkb_bundled(self, sys_lib_dir, sys_share_dir):
        xkb_bundled = False
        if not os.path.isdir(sys_lib_dir):
            return False
        for entry in os.listdir(sys_lib_dir):
            if entry.startswith('libxkbcommon.so'):
                xkb_bundled = True
                break
        if xkb_bundled:
            # FIXME: the xkb directory is only copied when --syslibs=bundle-all but not with --syslibs=generate-script and we may be including libxkbcommon.so in that case (jhbuild for example)
            if os.path.isdir(os.path.join(sys_share_dir, 'xkb')):
                return True
        return False


    def generate_wrapper_script(self, interpreter, binary_to_wrap, extra_environment_variables={}):
        if not os.path.isfile(os.path.join(self._destination_dir, 'bin', binary_to_wrap)):
            raise RuntimeError('Cannot find binary to wrap for %s' % binary_to_wrap)
        _log.info('Generate wrapper script %s' % binary_to_wrap)
        script_file = os.path.join(self._destination_dir, binary_to_wrap)
        lib_dir = os.path.join(self._destination_dir, 'lib')
        sys_lib_dir = os.path.join(self._destination_dir, 'sys/lib')
        sys_share_dir = os.path.join(self._destination_dir, 'sys/share')

        with open(script_file, 'w') as script_handle:
            script_handle.write('#!/bin/sh\n')
            script_handle.write('%s="$(dirname $(readlink -f $0))"\n' % self.VAR_MYDIR)

            if self._has_patched_interpreter_relpath:
                # The interprerter (section PT_INTERP in ELF binary) needs to be a pre-defined path
                # So the only way of using our interpreter in an unknown destination dir is to use relative paths
                # For relative paths to work, we need to CD into the main basedir, but doing that may break
                # any parameter the user has passed as a relative path to where she is working (like a relpath to some html file)
                # So we try to update the arguments resolving relative paths to absolute before executing the program
                # Trick to update the arguments inspired from https://unix.stackexchange.com/a/421160
                script_handle.write('# Shipped binaries have a relpath to the interpreter, so update passed args with fullpaths and cd to ${MYDIR}\n')
                script_handle.write('args_update_relpaths_to_abs() {\n')
                script_handle.write('    for arg in "$@"; do\n')
                script_handle.write('      [ "${arg}" = "${arg#/}" ] && [ -e "${arg}" ] && arg="$(readlink -f -- "${arg}")"\n')
                script_handle.write('      printf %s "${arg}" | sed "s/\'/\'\\\\\\\\\'\'/g;1s/^/\'/;\\$s/\\$/\' /"\n')
                script_handle.write('    done\n')
                script_handle.write('echo " "\n')
                script_handle.write('}\n')
                script_handle.write('eval "set -- $(args_update_relpaths_to_abs "$@")"\n')
                script_handle.write('cd "${MYDIR}"\n')

            if os.path.isfile(os.path.join(sys_share_dir, 'certs/bundle-ca-certificates.pem')):
                script_handle.write('# Try to use the system CA file if possible, otherwise use the bundled one\n')
                script_handle.write('for WEBKIT_TLS_CAFILE_PEM in /etc/ssl/certs/ca-certificates.crt /etc/ssl/cert.pem /etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem "${MYDIR}/sys/share/certs/bundle-ca-certificates.pem"; do\n')
                script_handle.write('    [ -f "${WEBKIT_TLS_CAFILE_PEM}" ] && grep -q "BEGIN CERTIFICATE" "${WEBKIT_TLS_CAFILE_PEM}" && export WEBKIT_TLS_CAFILE_PEM="${WEBKIT_TLS_CAFILE_PEM}" && break\n')
                script_handle.write('done\n')

            for var, value in extra_environment_variables.items():
                script_handle.write('export %s="%s"\n' % (var, value))

            if self.is_xkb_bundled(sys_lib_dir, sys_share_dir):
                script_handle.write('[ -d /usr/share/X11/xkb ] || export XKB_CONFIG_ROOT="${%s}/sys/share/xkb"\n' % self.VAR_MYDIR)

            ld_library_path = '${%s}/lib' % self.VAR_MYDIR
            if os.path.isdir(sys_lib_dir):
                ld_library_path += ':${%s}/sys/lib' % self.VAR_MYDIR

            # Update cache of gdk-pixbuf loaders if needed (first run or when the absolute paths on disk have changed).
            if os.path.isdir(os.path.join(sys_lib_dir, 'gdk-pixbuf/loaders')):
                script_handle.write('export GDK_PIXBUF_MODULEDIR="${%s}/sys/lib/gdk-pixbuf/loaders"\n' % self.VAR_MYDIR)
                script_handle.write('export GDK_PIXBUF_MODULE_FILE="${%s}/sys/lib/gdk-pixbuf/loaders.cache"\n' % self.VAR_MYDIR)
                script_handle.write('grep -sq "\\"${GDK_PIXBUF_MODULEDIR}/" "${GDK_PIXBUF_MODULE_FILE}" || LD_LIBRARY_PATH="%s" "${%s}/bin/gdk-pixbuf-query-loaders" --update-cache\n' % (ld_library_path, self.VAR_MYDIR))

            # LD_LIBRARY_PATH is set at the end, just before the exec() call and before calling programs bundled, otherwise it may break commands from the system like script's usage of sed/readlink/grep
            script_handle.write('export LD_LIBRARY_PATH="%s"\n' % ld_library_path)
            dlopenwrap_libname = 'dlopenwrap.so'
            if os.path.isfile(os.path.join(lib_dir, dlopenwrap_libname)):
                script_handle.write('export LD_PRELOAD="${%s}/lib/%s"\n' % (self.VAR_MYDIR, dlopenwrap_libname))

            # Prefix the program with the interpreter when we are bundling all (interpreter is copied) and the path to the interpreter isn't patched.
            # Otherwise prefer to not prefix it, because that allow the process to use a more meaningful progname.
            interpreter_basename = os.path.basename(interpreter)
            if os.path.isfile(os.path.join(lib_dir, interpreter_basename)) and not self._has_patched_interpreter_relpath:
                script_handle.write('INTERPRETER="${%s}/lib/%s"\n' % (self.VAR_MYDIR, interpreter_basename))
                script_handle.write('exec "${INTERPRETER}" "${%s}/bin/%s" "$@"\n' % (self.VAR_MYDIR, binary_to_wrap))
            else:
                script_handle.write('exec "${%s}/bin/%s" "$@"\n' % (self.VAR_MYDIR, binary_to_wrap))

        os.chmod(script_file, 0o755)

    # A C wrapper that we build statically works much better when bundling everything as we don't need to care
    # about breaking the system bash interpreter because of the LD_LIBRARY_PATH
    def generate_and_build_static_cwrapper(self, interpreter, binary_to_wrap, extra_environment_variables={}):
        if not os.path.isfile(os.path.join(self._destination_dir, 'bin', binary_to_wrap)):
            raise RuntimeError('Cannot find binary to wrap for %s' % binary_to_wrap)
        _log.info('Generate and build static C wrapper %s' % binary_to_wrap)
        wrapper_program = os.path.join(self._destination_dir, binary_to_wrap)
        lib_dir = os.path.join(self._destination_dir, 'lib')
        sys_lib_dir = os.path.join(self._destination_dir, 'sys/lib')
        sys_share_dir = os.path.join(self._destination_dir, 'sys/share')

        wrapper_source = wrapper_program + ".c"

        export_env_variables = '// export required environment variables\n'
        if os.path.isfile(os.path.join(lib_dir, 'dlopenwrap.so')):
            export_env_variables += '    set_env_with_mydir("LD_PRELOAD", "lib/dlopenwrap.so", mydir);\n'
        for var, value in extra_environment_variables.items():
            if value.startswith("${MYDIR}/"):
                # remove shell var mydir at start of string
                value = value.removeprefix("${MYDIR}/")
                export_env_variables += '    set_env_with_mydir("%s", "%s", mydir);\n' % (var, value)
            else:
                export_env_variables += '    setenv("%s", "%s", 1);\n' % (var, value)
            # double check we are not passing unexpected shell vars on the value
            assert("$" not in value)

        should_set_ca_file_value = 1 if os.path.isfile(os.path.join(sys_share_dir, 'certs/bundle-ca-certificates.pem')) else 0
        should_xkb_bundle_value = 1 if self.is_xkb_bundled(sys_lib_dir, sys_share_dir) else 0
        should_update_gdx_pixbuf_cache_value = 0
        if os.path.isdir(os.path.join(sys_lib_dir, 'gdk-pixbuf/loaders')):
            should_update_gdx_pixbuf_cache_value = 1
            export_env_variables += '    set_env_with_mydir("GDK_PIXBUF_MODULEDIR", "sys/lib/gdk-pixbuf/loaders", mydir);\n'
            export_env_variables += '    set_env_with_mydir("GDK_PIXBUF_MODULE_FILE", "sys/lib/gdk-pixbuf/loaders.cache", mydir);\n'
        set_ld_path_value = 'snprintf(ldpath, sizeof(ldpath), "%s/lib", mydir);'
        if os.path.isdir(sys_lib_dir):
            set_ld_path_value = 'snprintf(ldpath, sizeof(ldpath), "%s/lib:%s/sys/lib", mydir, mydir);'
        with open(wrapper_source, 'w') as wrapper_source_handle:
            wrapper_source_handle.write(WRAPPER_CSOURCE_TEMPLATE % {
                                        'relative_path_binary': os.path.join('bin', binary_to_wrap),
                                        'should_set_ca_file_value': should_set_ca_file_value,
                                        'should_xkb_bundle_value': should_xkb_bundle_value,
                                        'should_update_gdx_pixbuf_cache_value': should_update_gdx_pixbuf_cache_value,
                                        'set_ld_path_value': set_ld_path_value,
                                        'export_env_variables': export_env_variables
                                        })
        compiler_program = os.getenv('CC', default='gcc')
        self._run_cmd_or_log_fail([compiler_program, '-static', '-o', wrapper_program, wrapper_source])
