project(
  'os_lab2_thread_pool',
  'cpp',
  version : '1.0.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3'
  ]
)

thread_dep = dependency('threads')

executable(
  'thread_pool',
  'thread_pooling.cpp',
  dependencies : thread_dep,
  install : false
)
