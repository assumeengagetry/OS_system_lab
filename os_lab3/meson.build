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

# 检查 ucontext.h 是否可用（需要 libc6-dev）
if not cc.has_header('ucontext.h')
  error('ucontext.h not found. Install with: sudo apt install libc6-dev')
endif

# ── 主程序（任务二：极简协程库）──────────────────────────────
executable('coroutine',
  sources : 'coroutine.c',
  c_args : ['-Wall'],
  install : false
)


