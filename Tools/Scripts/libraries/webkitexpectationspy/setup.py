from setuptools import setup


def readme():
    with open('README.md') as f:
        return f.read()


setup(
    name='webkitexpectationspy',
    version='1.0.0',
    description='Test expectations management for WebKit',
    long_description=readme(),
    long_description_content_type='text/markdown',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: Other/Proprietary License',
        'Operating System :: MacOS',
        'Operating System :: POSIX :: Linux',
        'Natural Language :: English',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Topic :: Software Development :: Testing',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    keywords='webkit testing expectations',
    url='https://github.com/WebKit/WebKit/tree/main/Tools/Scripts/libraries/webkitexpectationspy',
    author='Apple Inc.',
    author_email='webkit-dev@lists.webkit.org',
    license='Modified BSD',
    packages=[
        'webkitexpectationspy',
        'webkitexpectationspy.suites',
        'webkitexpectationspy.plugins',
        'webkitexpectationspy.mocks',
        'webkitexpectationspy.tests',
    ],
    install_requires=[],
    extras_require={
        'webkit': ['webkitcorepy'],
    },
    python_requires='>=3.9',
    include_package_data=True,
    zip_safe=False,
)
