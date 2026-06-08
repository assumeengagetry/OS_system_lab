/*
 * memfs.c — 基于 FUSE 的最小可读写内存文件系统
 *
 * 使用 FUSE 3 API (FUSE_USE_VERSION 31)
 * 编译：gcc -Wall memfs.c $(pkg-config fuse3 --cflags --libs) -o memfs
 * 挂载：./memfs ~/memfs_mount
 * 卸载：fusermount3 -u ~/memfs_mount
 */

#define FUSE_USE_VERSION 31
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_FILES 64
#define MAX_NAME_LEN 127
#define MAX_FILE_SIZE 4096

typedef enum { NODE_UNUSED = 0, NODE_DIR, NODE_FILE } node_type_t;

typedef struct {
  int used;                    /* 当前槽位是否已经保存了一个文件 */
  char name[MAX_NAME_LEN + 1]; /* 文件名，不含路径开头的 '/' */
  char data[MAX_FILE_SIZE];    /* 文件内容缓冲区 */
  size_t size;                 /* 当前文件实际大小，单位字节 */
  node_type_t type;
  int parent;
} mem_node_t;

static mem_node_t files[MAX_FILES];

/* ──────────────── 辅助函数前置声明 ──────────────── */
static int find_file(const char *name);
static int find_free_slot(void);
static int path_to_name(const char *path, const char **name);

/* ══════════════════════════════════════════════════
 * FUSE 回调
 * ══════════════════════════════════════════════════ */

/*
 * getattr — 回答 VFS "这个路径是什么" 的问题。
 * 类似 stat(2)：填写 stbuf 后返回 0；路径不存在返回 -ENOENT。
 */

static void *memfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  (void)cfg;

  /* files[] 是静态全局数组，已被清零；只需设置根节点 */
  files[0].type = NODE_DIR;
  files[0].parent = -1;
  strncpy(files[0].name, "/", MAX_NAME_LEN);
  files[0].size = 0;
  return NULL;
}
static int memfs_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
  (void)fi;
  memset(stbuf, 0, sizeof(struct stat));

  /* TODO 1: 根目录 "/" 分支 */
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  /* 把 "/a.txt" 转成 "a.txt" */
  const char *name;
  int ret = path_to_name(path, &name);
  if (ret < 0)
    return ret;

  /* TODO 2: 查找文件 */
  int idx = find_file(name);
  if (idx < 0)
    return -ENOENT;

  /* TODO 3: 普通文件元数据 */
  stbuf->st_mode = S_IFREG | 0644;
  stbuf->st_nlink = 1;
  stbuf->st_size = (off_t)files[idx].size;
  return 0;
}

/*
 * readdir — 回答 VFS "目录里有哪些名字" 的问题。
 * ls 会触发它；把所有有效文件名通过 filler 交给 VFS。
 */
static int memfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;

  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);

  /* TODO 4: 遍历文件数组，把所有 used 的文件名加入目录列表 */
  for (int i = 0; i < MAX_FILES; i++) {
    if (files[i].used)
      filler(buf, files[i].name, NULL, 0, 0);
  }
  return 0;
}

/*
 * create — 创建一个新的普通文件。
 * echo "..." > file 对不存在的文件会触发 create，然后再调用 write。
 */
static int memfs_create(const char *path, mode_t mode,
                        struct fuse_file_info *fi) {
  (void)mode;
  (void)fi;

  const char *name;
  int ret = path_to_name(path, &name);
  if (ret < 0)
    return ret;

  /* TODO 5: 检查同名文件是否已经存在 */
  if (find_file(name) >= 0)
    return -EEXIST;

  /* TODO 6: 找空槽位 */
  int idx = find_free_slot();
  if (idx < 0)
    return -ENOSPC;

  /* TODO 7: 初始化新文件槽位 */
  files[idx].used = 1;
  strncpy(files[idx].name, name, MAX_NAME_LEN);
  files[idx].name[MAX_NAME_LEN] = '\0';
  files[idx].size = 0;
  memset(files[idx].data, 0, MAX_FILE_SIZE);
  return 0;
}

/*
 * open — 打开已有文件；基础任务只需确认文件存在。
 */
static int memfs_open(const char *path, struct fuse_file_info *fi) {
  (void)fi;

  const char *name;
  int ret = path_to_name(path, &name);
  if (ret < 0)
    return ret;

  /* TODO 8: 文件存在则返回 0，否则返回 -ENOENT */
  if (find_file(name) < 0)
    return -ENOENT;
  return 0;
}

/*
 * read — 从文件中读取一段数据。
 * cat 会多次调用 read，每次带不同的 offset 和 size。
 */
static int memfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  (void)fi;

  const char *name;
  int ret = path_to_name(path, &name);
  if (ret < 0)
    return ret;

  /* TODO 9: 实现普通文件读取 */

  /* 1. 确认文件存在 */
  int idx = find_file(name);
  if (idx < 0)
    return -ENOENT;

  /* 2. offset 合法性判断 */
  if (offset < 0)
    return -EINVAL;

  /* 3. offset 已越过文件末尾：返回 0（EOF） */
  if ((size_t)offset >= files[idx].size)
    return 0;

  /* 4. 裁剪 size，只读到文件末尾 */
  size_t avail = files[idx].size - (size_t)offset;
  if (size > avail)
    size = avail;

  /* 5. 复制数据 */
  memcpy(buf, files[idx].data + offset, size);
  return (int)size;
}

/*
 * write — 向文件中写入一段数据。
 * 覆盖写和追加写都通过 offset 表达；写后如果超过原大小则扩展文件。
 */
static int memfs_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
  (void)fi;

  const char *name;
  int ret = path_to_name(path, &name);
  if (ret < 0)
    return ret;

  /* TODO 10: 实现普通文件写入 */

  /* 1. 确认文件存在 */
  int idx = find_file(name);
  if (idx < 0)
    return -ENOENT;

  /* 2. offset 合法性判断 */
  if (offset < 0)
    return -EINVAL;

  /* 3. 检查写入后是否超出 MAX_FILE_SIZE */
  if ((size_t)offset >= MAX_FILE_SIZE)
    return -EFBIG;
  if (offset + (off_t)size > MAX_FILE_SIZE)
    size = MAX_FILE_SIZE - (size_t)offset; /* 截断到最大允许长度 */
  if (size == 0)
    return -EFBIG;

  /* 4. 把字节写入 offset 对应的位置 */
  memcpy(files[idx].data + offset, buf, size);

  /* 5. 如果写入后文件变长，更新大小 */
  if ((size_t)offset + size > files[idx].size)
    files[idx].size = (size_t)offset + size;

  /* 6. 返回实际写入字节数 */
  return (int)size;
}

/*
 * truncate — 改变文件长度。
 * 缩小时截断末尾；放大时新增区域清零。
 *
 * FUSE 3 的 truncate 回调多了一个 fi 参数。
 */
static int memfs_truncate(const char *path, off_t size,
                          struct fuse_file_info *fi) {
  (void)fi;

  const char *name;
  int ret = path_to_name(path, &name);
  if (ret < 0)
    return ret;

  /* TODO 11: 实现文件长度调整 */

  /* 1. 确认文件存在 */
  int idx = find_file(name);
  if (idx < 0)
    return -ENOENT;

  /* 2. size 合法性判断 */
  if (size < 0)
    return -EINVAL;
  if (size > MAX_FILE_SIZE)
    return -EFBIG;

  /* 3. 放大时，新增区域清零 */
  if ((size_t)size > files[idx].size)
    memset(files[idx].data + files[idx].size, 0,
           (size_t)size - files[idx].size);

  /* 4. 更新文件大小 */
  files[idx].size = (size_t)size;
  return 0;
}

/*
 * unlink — 删除普通文件。
 * rm 会触发它；删除后文件不应再出现在 readdir、getattr、open 中。
 */
static int memfs_unlink(const char *path) {
  const char *name;
  int ret = path_to_name(path, &name);
  if (ret < 0)
    return ret;

  /* TODO 12: 实现文件删除 */

  /* 1. 确认文件存在 */
  int idx = find_file(name);
  if (idx < 0)
    return -ENOENT;

  /* 2. 将槽位恢复为未使用状态 */
  files[idx].used = 0;
  memset(files[idx].name, 0, sizeof(files[idx].name));
  memset(files[idx].data, 0, MAX_FILE_SIZE);
  files[idx].size = 0;
  return 0;
}

/* ══════════════════════════════════════════════════
 * 辅助函数
 * ══════════════════════════════════════════════════ */

/*
 * find_file — 按文件名查找有效槽位。
 * 找到返回下标；找不到返回 -1。
 */
static int find_file(const char *name) {
  /* TODO 13 & 14 */
  for (int i = 0; i < MAX_FILES; i++) {
    if (files[i].used && strcmp(files[i].name, name) == 0)
      return i;
  }
  return -1;
}

/*
 * find_free_slot — 找一个未使用的槽位。
 * 找到返回下标；表满返回 -1。
 */
static int find_free_slot(void) {
  /* TODO 15 & 16 */
  for (int i = 0; i < MAX_FILES; i++) {
    if (!files[i].used)
      return i;
  }
  return -1;
}

/*
 * path_to_name — 把 FUSE 路径转成内部文件名。
 * "/a.txt" → *name = "a.txt"，返回 0。
 * "/" 或 "/dir/a.txt" 等不合法路径返回负错误码。
 */
static int path_to_name(const char *path, const char **name) {
  /* TODO 17: 格式检查 —— 必须是 "/" 开头的一级路径 */
  if (path == NULL || path[0] != '/')
    return -ENOENT;

  /* 跳过开头的 '/' */
  const char *p = path + 1;

  /* 空文件名（即根目录 "/"）：不是普通文件 */
  if (*p == '\0')
    return -EISDIR;

  /* 多级路径含有 '/'：不支持 */
  if (strchr(p, '/') != NULL)
    return -ENOENT;

  /* TODO 18: 文件名长度检查 */
  if (strlen(p) > MAX_NAME_LEN)
    return -ENAMETOOLONG;

  /* TODO 19: 返回文件名部分 */
  *name = p;
  return 0;
}

static int look_up_path(char *path) {

  if (path == NULL || path[0] != '/')
    return -ENOENT;

  /* 跳过开头的 '/' */
  const char *p = path + 1;

  /* 空文件名（即根目录 "/"）：不是普通文件 */
  if (*p == '\0')
    return 0;
}
/* ══════════════════════════════════════════════════
 * FUSE 操作表 & main
 * ══════════════════════════════════════════════════ */

static const struct fuse_operations memfs_oper = {
    .getattr = memfs_getattr,
    .readdir = memfs_readdir,
    .create = memfs_create,
    .open = memfs_open,
    .read = memfs_read,
    .write = memfs_write,
    .truncate = memfs_truncate,
    .unlink = memfs_unlink,
};

int main(int argc, char *argv[]) {
  return fuse_main(argc, argv, &memfs_oper, NULL);
}
