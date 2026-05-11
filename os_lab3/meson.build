project('coroutine', 'cpp',
  version : '1.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debug',
  ],
)

cc = meson.get_compiler('cpp')
fs = import('fs')

# 检查 ucontext.h 是否可用（需要 libc6-dev）
if not cc.has_header('ucontext.h')
  error('ucontext.h not found. Install with: sudo apt install libc6-dev')
endif

# ── 主程序（任务二：极简协程库）──────────────────────────────
executable('coroutine',
  sources : 'coroutine.cpp',
  c_args  : ['-Wall', '-Wextra'],
  install : false,
)

# ── 挑战 A：抢占式调度（可选，文件存在时才编译）────────────────
if fs.exists('coroutine_preempt.c')
  executable('coroutine_preempt',
    sources : 'coroutine_preempt.c',
    c_args  : ['-Wall', '-Wextra'],
    install : false,
  )
endif

# ── 挑战 B：时间感知调度（可选，文件存在时才编译）───────────────
if fs.exists('coroutine_sleep.c')
  executable('coroutine_sleep',
    sources : 'coroutine_sleep.c',
    c_args  : ['-Wall', '-Wextra'],
    install : false,
  )
endif
