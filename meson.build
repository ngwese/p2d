project('p2d', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_P2D', '-Wzero-length-array']

lib_src = [
  'src/p2d.cpp',
  'src/result.cpp',
  'src/driver.cpp',
]

lib_pub_incname = 'p2d'

lib_pub_incdir = include_directories(lib_pub_incname)
lib_priv_incdir = include_directories('src')
lib_incdirs = [lib_pub_incdir, lib_priv_incdir]

lib_dependencies = [
  dependency('libusb-1.0', required: true, method: 'pkg-config', version: '>=1.0'),
]

shlib = shared_library('p2d', lib_src,
  install : true,
  cpp_args : lib_args,
  include_directories: lib_incdirs,
  dependencies: lib_dependencies,
  gnu_symbol_visibility : 'hidden',
)

test_src = [
  'test/p2d_test.cpp',
]

test_exe = executable('p2d_test', test_src,
  include_directories: lib_incdirs,
  link_with : shlib)

test('p2d', test_exe)

# Make this library usable as a Meson subproject.
p2d_dep = declare_dependency(
  include_directories: lib_pub_incdir,
  link_with : shlib)

# Make this library usable from the system's
# package manager.

lib_hdrs = [
  lib_pub_incname / 'bitmap.hpp',
  lib_pub_incname / 'display.hpp',
  lib_pub_incname / 'driver.hpp',
  lib_pub_incname / 'p2d.hpp',
  lib_pub_incname / 'result.hpp',
]

install_headers(lib_hdrs, subdir : 'p2d')

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name : 'p2d',
  filebase : 'p2d',
  description : 'Ableton Push2 interfacing library',
  subdirs : 'p2d',
  libraries : shlib,
  version : '0.1',
)
