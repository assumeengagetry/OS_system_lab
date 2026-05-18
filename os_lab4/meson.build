  project('os_lab4', 'cpp',
  version : '1.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debug',
  ],
)

cc = meson.get_compiler('cpp')
fs = import('fs')

shared_library('hook_malloc', ['mm.cpp', 'hook_malloc.cpp'],
               dependencies: dependency('threads'))
