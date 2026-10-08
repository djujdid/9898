/*
 * regex.c
 * 功能: 通过 mmap 内存映射读取 url.html(可通过命令行参数指定文件), 使用 POSIX
 *       正则函数 (regcomp / regexec) 匹配其中的 <a> 标签, 尽可能匹配完全,
 *       匹配成功后提取"新闻地址"(href) 与"新闻标题"(标签内文字, 若为空则回退
 *       title 属性), 并打印输出数据。
 *
 * 读取方式: 采用作业 demo 的 mmap 内存映射方案
 *   open  -> lseek(SEEK_END) 取文件大小 -> mmap 映射 -> close(fd)
 *   mmap_ptr 指向的映射内存即包含文件全部数据, 直接作为正则匹配的数据源。
 *   (MAP_PRIVATE + PROT_READ|PROT_WRITE, 映射区可写, 便于就地 trim)
 *   映射长度申请 fsize+1 字节, 超出文件长度部分由内核补零, 保证 '\0' 结尾,
 *   使 regexec / strchr 等以 C 字符串处理映射内存时不会越界。
 *
 * 正则(ERE)设计要点, 兼顾可用性:
 *   <a[^>]*href="([^"]*)"[^>]*>([^<]*)</a>
 *   - <a[^>]*        : 允许 <a 后 href 之前存在 class 等其它属性
 *   - href="([^"]*)" : 捕获新闻地址(第1个捕获组)
 *   - [^>]*>         : 跳过 target / title 等其余属性直到开标签结束
 *   - ([^<]*)        : 捕获标签内文字标题(第2个捕获组), 不含子标签
 *   - </a>           : 闭合标签
 *   若标签内文字为空, 再单独用 title="([^"]*)" 提取标题, 提升匹配完整度。
 */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

/* 通过 mmap 把整个文件映射进内存, 返回映射指针(内容以 '\0' 结尾); 失败返回 NULL */
static char *mmap_file(const char *path, size_t *out_size)
{
    /* 1. 打开文件 */
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        /* O_RDWR 失败时(如文件只读)回退为只读方式 */
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "无法打开文件: %s\n", path);
            return NULL;
        }
    }

    /* 2. 获取文件大小 */
    off_t fsize = lseek(fd, 0, SEEK_END);
    if (fsize <= 0) {
        fprintf(stderr, "文件为空或大小获取失败: %s\n", path);
        close(fd);
        return NULL;
    }

    /* 3. 文件映射(申请 fsize+1 字节, 尾部由内核补零, 保证 C 字符串安全) */
    char *mmap_ptr = NULL;
    mmap_ptr = (char *)mmap(NULL, (size_t)fsize + 1,
                            PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);   /* 映射建立后 fd 即可关闭, 映射仍然有效 */

    if (mmap_ptr == MAP_FAILED) {
        perror("mmap 映射失败");
        return NULL;
    }

    /* 超出文件长度的那一页内字节为内核补零, mmap_ptr[fsize] 即 '\0' */
    if (out_size) *out_size = (size_t)fsize;
    return mmap_ptr;
}

/* 去除首尾 ASCII 空白(就地修改, 依赖映射区可写) */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == ' ' || s[len - 1] == '\t' ||
            s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
    return s;
}

/* 从给定的开标签片段中, 用 title="..." 提取标题(回退方案) */
static int extract_title_attr(const char *open_tag, size_t tag_len,
                              char *out, size_t out_sz)
{
    static regex_t re;
    static int compiled = 0;
    if (!compiled) {
        if (regcomp(&re, "title=\"([^\"]*)\"", REG_EXTENDED) != 0)
            return 0;
        compiled = 1;
    }

    /* 仅在开标签范围内查找, 用临时副本避免越界 */
    char *tmp = (char *)malloc(tag_len + 1);
    if (!tmp) return 0;
    memcpy(tmp, open_tag, tag_len);
    tmp[tag_len] = '\0';

    regmatch_t m[2];
    int ok = 0;
    if (regexec(&re, tmp, 2, m, 0) == 0) {
        size_t s = m[1].rm_so, e = m[1].rm_eo;
        size_t cplen = e - s;
        if (cplen >= out_sz) cplen = out_sz - 1;
        memcpy(out, tmp + s, cplen);
        out[cplen] = '\0';
        ok = 1;
    }
    free(tmp);
    return ok;
}

int main(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "url.html";

    /* 数据源: mmap_ptr 指向的映射内存包含文件的所有数据 */
    size_t fsize = 0;
    char *mmap_ptr = mmap_file(path, &fsize);
    if (!mmap_ptr) return 1;

    const char *pattern = "<a[^>]*href=\"([^\"]*)\"[^>]*>([^<]*)</a>";
    regex_t reg;
    if (regcomp(&reg, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "正则编译失败\n");
        munmap(mmap_ptr, fsize + 1);
        return 1;
    }

    printf("======== <a> 标签匹配结果 (文件: %s, mmap 映射 %zu 字节) ========\n",
           path, fsize);
    printf("%-4s  %-70s  %s\n", "序号", "新闻地址(href)", "新闻标题");
    printf("--------------------------------------------------------------------------\n");

    const char *p = mmap_ptr;
    int count = 0;
    regmatch_t matches[3];
    while (regexec(&reg, p, 3, matches, 0) == 0) {
        /* 提取 href */
        size_t hs = matches[1].rm_so, he = matches[1].rm_eo;
        size_t hlen = he - hs;
        char *href = (char *)malloc(hlen + 1);
        memcpy(href, p + hs, hlen);
        href[hlen] = '\0';

        /* 提取标签内文字标题 */
        size_t ts = matches[2].rm_so, te = matches[2].rm_eo;
        size_t tlen = te - ts;
        char *title = (char *)malloc(tlen + 1);
        memcpy(title, p + ts, tlen);
        title[tlen] = '\0';
        char *title_trim = trim(title);

        /* 整段 <a ...> 开标签的起点(用于回退提取 title 属性) */
        const char *match_start = p + matches[0].rm_so;
        const char *gt = strchr(match_start, '>');
        size_t open_tag_len = gt ? (size_t)(gt - match_start + 1) : 0;

        char title_attr[1024] = {0};
        int has_attr = extract_title_attr(match_start, open_tag_len,
                                          title_attr, sizeof(title_attr));

        /* 标题: 优先标签内文字, 为空则用 title 属性; 都为空则跳过(空锚点) */
        if (title_trim[0] == '\0') {
            if (has_attr && title_attr[0] != '\0') {
                title_trim = title_attr;
            } else {
                free(href);
                free(title);
                p += matches[0].rm_eo;
                continue;   /* 跳过没有标题的空链接 */
            }
        }

        count++;
        printf("%-4d  %-70s  %s\n", count, href, title_trim);

        free(href);
        free(title);
        p += matches[0].rm_eo;
    }

    printf("--------------------------------------------------------------------------\n");
    printf("匹配成功并提取的新闻条数: %d\n", count);

    regfree(&reg);
    munmap(mmap_ptr, fsize + 1);   /* 释放映射 */
    return 0;
}
