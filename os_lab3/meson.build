project('coroutine', 'c',
  version : '1.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',

    'buildtype=debug',
  ],
)

cc = meson.get_compiler('c')
fs = import('fs')
# 例如启用 POSIX.1-2008 标准扩展
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: ['c', 'cpp'])

executable('coroutine',
  sources : 'coroutine.c',
  c_args : ['-Wall'],
  install : false
)


