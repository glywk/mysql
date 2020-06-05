#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

from sys import argv
from subprocess import check_call
from os import mkdir, chdir, makedirs

VERSION_TAG = 'release-1.10.0'

def usage():
    print('Usage: {} <install-dir>'.format(argv[0]))
    exit(1)

def main():
    if len(argv) != 2:
        usage()
    install_dir = argv[1]
    print('Installing gtest in ', install_dir)
    makedirs(install_dir, exist_ok=True)
    check_call(['git', 'clone', 'https://github.com/google/googletest.git'])
    chdir('googletest')
    check_call(['git', 'checkout', VERSION_TAG])
    
    for build_type in ('Debug', 'Release'):
        build_dir = 'build-' + build_type
        mkdir(build_dir)
        chdir(build_dir)
        check_call([
            'cmake',
            '-G',
            'Ninja',
            '-Dgtest_force_shared_crt=ON',
            '-DCMAKE_BUILD_TYPE={}'.format(build_type),
            '-DCMAKE_INSTALL_PREFIX={}'.format(install_dir),
            '..'
        ])
        check_call(['cmake', '--build', '.', '--target', 'install', '-j'])
        chdir('..')
    
if __name__ == '__main__':
    main()
