project('os_lab4', 'c',
        version : '1.0',
        default_options : [
          'c_std=c11',
          'warning_level=2',
          'buildtype=debug',
        ],
)

fs = import('fs')

shared_library('hook_malloc', ['my_malloc.c', 'hook_malloc.c'],
               dependencies: dependency('threads'))

# 测试可执行文件
test_exe = executable('test_malloc',
                      ['test_my_malloc.c', 'my_malloc.c'],
)

# 注册为 meson test
test('my_malloc_tests', test_exe)