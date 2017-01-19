# This file contains dependencies for WebRTC that are not shared with Chromium.
# If you wish to add a dependency that is present in Chromium's src/DEPS or a
# directory from the Chromium checkout, you should add it to setup_links.py
# instead.

vars = {
  'extra_gyp_flag': '-Dextra_gyp_flag=0',
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '04e7c673d985877c240d0c2791c33e9aeeace7f8',
}

# NOTE: Use http rather than https; the latter can cause problems for users
# behind proxies.
deps = {
  'src/third_party/gflags/src':
    Var('chromium_git') + '/external/github.com/gflags/gflags@03bebcb065c83beff83d50ae025a55a4bf94dfca',
}

deps_os = {
  'win': {
    'src/third_party/winsdk_samples/src':
      Var('chromium_git') + '/external/webrtc/deps/third_party/winsdk_samples_v71@e71b549167a665d7424d6f1dadfbff4b4aad1589',
  },
}

hooks = [
  {
    # Check for legacy named top-level dir (named 'trunk').
    'name': 'check_root_dir_name',
    'pattern': '.',
    'action': ['python','-c',
               ('import os,sys;'
                'script = os.path.join("trunk","check_root_dir.py");'
                '_ = os.system("%s %s" % (sys.executable,script)) '
                'if os.path.exists(script) else 0')],
  },
  {
    # Clone chromium and its deps.
    'name': 'sync chromium',
    'pattern': '.',
    'action': ['python', '-u', 'src/sync_chromium.py',
               '--target-revision', Var('chromium_revision')],
  },
  {
    # Create links to shared dependencies in Chromium.
    'name': 'setup_links',
    'pattern': '.',
    'action': ['python', 'src/setup_links.py'],
  },
  {
    # This clobbers when necessary (based on get_landmines.py). It should be
    # an early hook but it will need to be run after syncing Chromium and
    # setting up the links, so the script actually exists.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python',
        'src/build/landmines.py',
        '--landmine-scripts',
        'src/webrtc/build/get_landmines.py',
        '--src-dir',
        'src',
    ],
  },
  {
     # Pull sanitizer-instrumented third-party libraries if requested via
     # GYP_DEFINES. This could be done as part of sync_chromium.py above
     # but then we would need to run all the Chromium hooks each time,
     # which will slow things down a lot.
     'name': 'instrumented_libraries',
     'pattern': '\\.sha1',
     'action': ['python', 'src/third_party/instrumented_libraries/scripts/download_binaries.py'],
   },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    'pattern': '.',
    'action': ['download_from_google_storage',
               '--directory',
               '--recursive',
               '--num_threads=10',
               '--no_auth',
               '--quiet',
               '--bucket', 'chromium-webrtc-resources',
               'src/resources'],
  },
]

