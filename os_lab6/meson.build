project('exp6', 'c',
  default_options: ['c_std=c11', 'warning_level=2']
)

# 依赖 fuse3（Ubuntu: sudo apt install libfuse3-dev）
fuse3_dep = dependency('fuse3')

# 基础任务：最小可读写内存文件系统
executable('memfs',
  'memfs.c',
  dependencies: fuse3_dep,
  install: false
)
