from distutils.core import setup, Extension

module = Extension(
    'signposts',
    sources=['signposts.c'],
)

setup(
    name='signposts',
    version='1.0.0',
    description='provides python api for emitting os_signposts while running a browser benchmark',
    ext_modules=[module],
)
