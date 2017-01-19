vars = {
  # Override root_dir in your .gclient's custom_vars to specify a custom root
  # folder name.
  'root_dir': 'libyuv',
  'extra_gyp_flag': '-Dextra_gyp_flag=0',
  'chromium_git': 'https://chromium.googlesource.com',

  # Roll the Chromium Git hash to pick up newer versions of all the
  # dependencies and tools linked to in setup_links.py.
  'chromium_revision': '941118827f5240dedb40082cffb1ead6c6d621cc',
}

# NOTE: Use http rather than https; the latter can cause problems for users
# behind proxies.
deps = {
  Var('root_dir') + '/third_party/gflags/src':
    Var('chromium_git') + '/external/github.com/gflags/gflags@03bebcb065c83beff83d50ae025a55a4bf94dfca',
}

# Define rules for which include paths are allowed in our source.
include_rules = [ '+gflags' ]

hooks = [
  {
    # Clone chromium and its deps.
    'name': 'sync chromium',
    'pattern': '.',
    'action': ['python', '-u', Var('root_dir') + '/sync_chromium.py',
               '--target-revision', Var('chromium_revision')],
  },
  {
    # Create links to shared dependencies in Chromium.
    'name': 'setup_links',
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/setup_links.py'],
  },
  {
    # This clobbers when necessary (based on get_landmines.py). It should be
    # an early hook but it will need to be run after syncing Chromium and
    # setting up the links, so the script actually exists.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python',
        Var('root_dir') + '/build/landmines.py',
        '--landmine-scripts',
        Var('root_dir') + '/tools/get_landmines.py',
        '--src-dir',
        Var('root_dir'),
    ],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'pattern': '.',
    'action': ['python', Var('root_dir') + '/gyp_libyuv'],
  },
]
