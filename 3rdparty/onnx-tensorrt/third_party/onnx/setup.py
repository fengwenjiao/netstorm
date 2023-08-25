from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from distutils.spawn import find_executable
from distutils import sysconfig, log
import setuptools
import setuptools.command.build_py
import setuptools.command.develop
import setuptools.command.build_ext

from collections import namedtuple
from contextlib import contextmanager
import glob
import os
import shlex
import subprocess
import sys
import struct
from textwrap import dedent
import multiprocessing


TOP_DIR = os.path.realpath(os.path.dirname(__file__))
SRC_DIR = os.path.join(TOP_DIR, 'onnx')
TP_DIR = os.path.join(TOP_DIR, 'third_party')
CMAKE_BUILD_DIR = os.path.join(TOP_DIR, '.setuptools-cmake-build')

WINDOWS = (os.name == 'nt')

CMAKE = find_executable('cmake3') or find_executable('cmake')
MAKE = find_executable('make')

install_requires = []
setup_requires = []
tests_require = []
extras_require = {}

################################################################################
# Global variables for controlling the build variant
################################################################################

ONNX_ML = bool(os.getenv('ONNX_ML') == '1')
ONNX_NAMESPACE = os.getenv('ONNX_NAMESPACE', 'onnx')
ONNX_BUILD_TESTS = bool(os.getenv('ONNX_BUILD_TESTS') == '1')

DEBUG = bool(os.getenv('DEBUG'))
COVERAGE = bool(os.getenv('COVERAGE'))

################################################################################
# Version
################################################################################

try:
    git_version = subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                          cwd=TOP_DIR).decode('ascii').strip()
except (OSError, subprocess.CalledProcessError):
    git_version = None

with open(os.path.join(TOP_DIR, 'VERSION_NUMBER')) as version_file:
    VersionInfo = namedtuple('VersionInfo', ['version', 'git_version'])(
        version=version_file.read().strip(),
        git_version=git_version
    )

################################################################################
# Pre Check
################################################################################

assert CMAKE, 'Could not find "cmake" executable!'

################################################################################
# Utilities
################################################################################


@contextmanager
def cd(path):
    if not os.path.isabs(path):
        raise RuntimeError('Can only cd to absolute path, got: {}'.format(path))
    orig_path = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(orig_path)


################################################################################
# Customized commands
################################################################################


class ONNXCommand(setuptools.Command):
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass


class create_version(ONNXCommand):
    def run(self):
        with open(os.path.join(SRC_DIR, 'version.py'), 'w') as f:
            f.write(dedent('''\
            # This file is generated by setup.py. DO NOT EDIT!

            from __future__ import absolute_import
            from __future__ import division
            from __future__ import print_function
            from __future__ import unicode_literals

            version = '{version}'
            git_version = '{git_version}'
            '''.format(**dict(VersionInfo._asdict()))))


class cmake_build(setuptools.Command):
    """
    Compiles everything when `python setupmnm.py build` is run using cmake.

    Custom args can be passed to cmake by specifying the `CMAKE_ARGS`
    environment variable.

    The number of CPUs used by `make` can be specified by passing `-j<ncpus>`
    to `setup.py build`.  By default all CPUs are used.
    """
    user_options = [
        (str('jobs='), str('j'), str('Specifies the number of jobs to use with make'))
    ]

    built = False

    def initialize_options(self):
        self.jobs = multiprocessing.cpu_count()

    def finalize_options(self):
        self.jobs = int(self.jobs)

    def run(self):
        if cmake_build.built:
            return
        cmake_build.built = True
        if not os.path.exists(CMAKE_BUILD_DIR):
            os.makedirs(CMAKE_BUILD_DIR)

        with cd(CMAKE_BUILD_DIR):
            # configure
            cmake_args = [
                CMAKE,
                '-DPYTHON_INCLUDE_DIR={}'.format(sysconfig.get_python_inc()),
                '-DPYTHON_EXECUTABLE={}'.format(sys.executable),
                '-DBUILD_ONNX_PYTHON=ON',
                '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                '-DONNX_NAMESPACE={}'.format(ONNX_NAMESPACE),
                '-DPY_EXT_SUFFIX={}'.format(sysconfig.get_config_var('EXT_SUFFIX') or ''),
            ]
            if COVERAGE:
                cmake_args.append('-DONNX_COVERAGE=ON')
            if COVERAGE or DEBUG:
                # in order to get accurate coverage information, the
                # build needs to turn off optimizations
                cmake_args.append('-DCMAKE_BUILD_TYPE=Debug')
            if WINDOWS:
                cmake_args.extend([
                    # we need to link with libpython on windows, so
                    # passing python version to window in order to
                    # find python in cmake
                    '-DPY_VERSION={}'.format('{0}.{1}'.format(*sys.version_info[:2])),
                    '-DONNX_USE_MSVC_STATIC_RUNTIME=ON',
                ])
                if 8 * struct.calcsize("P") == 64:
                    # Temp fix for CI
                    # TODO: need a better way to determine generator
                    cmake_args.append('-DCMAKE_GENERATOR_PLATFORM=x64')
            if ONNX_ML:
                cmake_args.append('-DONNX_ML=1')
            if ONNX_BUILD_TESTS:
                cmake_args.append('-DONNX_BUILD_TESTS=ON')
            if 'CMAKE_ARGS' in os.environ:
                extra_cmake_args = shlex.split(os.environ['CMAKE_ARGS'])
                # prevent crossfire with downstream scripts
                del os.environ['CMAKE_ARGS']
                log.info('Extra cmake args: {}'.format(extra_cmake_args))
                cmake_args.extend(extra_cmake_args)
            cmake_args.append(TOP_DIR)
            subprocess.check_call(cmake_args)

            build_args = [CMAKE, '--build', os.curdir]
            if WINDOWS:
                build_args.extend(['--', '/maxcpucount:{}'.format(self.jobs)])
            else:
                build_args.extend(['--', '-j', str(self.jobs)])
            subprocess.check_call(build_args)


class build_py(setuptools.command.build_py.build_py):
    def run(self):
        self.run_command('create_version')
        self.run_command('cmake_build')

        generated_python_files = \
            glob.glob(os.path.join(CMAKE_BUILD_DIR, 'onnx', '*.py')) + \
            glob.glob(os.path.join(CMAKE_BUILD_DIR, 'onnx', '*.pyi'))

        for src in generated_python_files:
            dst = os.path.join(
                TOP_DIR, os.path.relpath(src, CMAKE_BUILD_DIR))
            self.copy_file(src, dst)

        return setuptools.command.build_py.build_py.run(self)


class develop(setuptools.command.develop.develop):
    def run(self):
        self.run_command('build_py')
        setuptools.command.develop.develop.run(self)


class build_ext(setuptools.command.build_ext.build_ext):
    def run(self):
        self.run_command('cmake_build')
        setuptools.command.build_ext.build_ext.run(self)

    def build_extensions(self):
        for ext in self.extensions:
            fullname = self.get_ext_fullname(ext.name)
            filename = os.path.basename(self.get_ext_filename(fullname))

            lib_path = CMAKE_BUILD_DIR
            if os.name == 'nt':
                debug_lib_dir = os.path.join(lib_path, "Debug")
                release_lib_dir = os.path.join(lib_path, "Release")
                if os.path.exists(debug_lib_dir):
                    lib_path = debug_lib_dir
                elif os.path.exists(release_lib_dir):
                    lib_path = release_lib_dir
            src = os.path.join(lib_path, filename)
            dst = os.path.join(os.path.realpath(self.build_lib), "onnx", filename)
            self.copy_file(src, dst)


class mypy_type_check(ONNXCommand):
    description = 'Run MyPy type checker'

    def run(self):
        """Run command."""
        onnx_script = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "tools/mypy-onnx.py"))
        returncode = subprocess.call([sys.executable, onnx_script])
        sys.exit(returncode)


cmdclass = {
    'create_version': create_version,
    'cmake_build': cmake_build,
    'build_py': build_py,
    'develop': develop,
    'build_ext': build_ext,
    'typecheck': mypy_type_check,
}

################################################################################
# Extensions
################################################################################

ext_modules = [
    setuptools.Extension(
        name=str('onnx.onnx_cpp2py_export'),
        sources=[])
]

################################################################################
# Packages
################################################################################

# no need to do fancy stuff so far
packages = setuptools.find_packages()

install_requires.extend([
    'protobuf',
    'numpy',
    'six',
    'typing>=3.6.4',
    'typing-extensions>=3.6.2.1',
])

################################################################################
# Test
################################################################################

setup_requires.append('pytest-runner')
tests_require.append('pytest')
tests_require.append('nbval')
tests_require.append('tabulate')
tests_require.append('typing')
tests_require.append('typing-extensions')

if sys.version_info[0] == 3:
    # Mypy doesn't work with Python 2
    extras_require['mypy'] = ['mypy==0.600']

################################################################################
# Final
################################################################################

setuptools.setup(
    name="onnx",
    version=VersionInfo.version,
    description="Open Neural Network Exchange",
    ext_modules=ext_modules,
    cmdclass=cmdclass,
    packages=packages,
    include_package_data=True,
    install_requires=install_requires,
    setup_requires=setup_requires,
    tests_require=tests_require,
    extras_require=extras_require,
    author='bddppq',
    author_email='jbai@fb.com',
    url='https://github.com/onnx/onnx',
    entry_points={
        'console_scripts': [
            'check-model = onnx.bin.checker:check_model',
            'check-node = onnx.bin.checker:check_node',
            'backend-test-tools = onnx.backend.test.cmd_tools:main',
        ]
    },
)
