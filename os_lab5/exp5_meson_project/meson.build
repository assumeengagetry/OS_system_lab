project('exp5', 'c',
  default_options: ['c_std=c11', 'warning_level=2']
)

# 任务一：观察 Linux 真实虚拟内存行为
executable('vm_observe',
  'vm_observe.c',
  install: false
)

# 任务二基础版：FIFO + swap
# 如果老师要求源文件名必须是 mini_vmem.c，可以把 mini_vmem_fifo.c 改名为 mini_vmem.c，
# 然后把这里的源文件名也改成 'mini_vmem.c'。
executable('mini_vmem',
  'mini_vmem_fifo.c',
  install: false
)

# 附加题一：Second Chance 页面置换
executable('mini_vmem_second_chance',
  'mini_vmem_second_chance.c',
  install: false
)

# 附加题二：写保护 Dirty Tracking
executable('mini_vmem_dirty_tracking',
  'mini_vmem_dirty_tracking.c',
  install: false
)
