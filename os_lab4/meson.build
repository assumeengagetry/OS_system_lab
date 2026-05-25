  project('os_lab4', 'c',
  version : '1.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debug',
  ],
)

 fs = import('fs')

shared_library('hook_malloc', ['mm.c', 'hook_malloc.c'],
               dependencies: dependency('threads'))
