#ifndef _UTILS_H_
#define _UTILS_H_

// http://zh.wikipedia.org/wiki/C99#.E5.BA.93
#include <stdio.h>
#include <stdlib.h>         /* For srandom, random */
#include <string.h>         // For strlen, strerror;
#include <syslog.h>         // For about LOG_*;
#include <stdarg.h>         // For va_list...;
#include <signal.h>         // For signal, sigaction...;
#include <sys/stat.h>       // For struct stat, fstat, lstat...; 
#include <sys/types.h>      // For int8_t, int16_t, int32_t, int64_t...;
#include <stdint.h>         // For uint8_t...;
#include <unistd.h>         /* For pid_t, socklen_t, SEEK_SET, read, write,
                             * access, close, pipe, alarm... */
#include <fcntl.h>          /* For F_GETFL, O_NONBLOCK, F_SETFL, O_WRONLYO_WRONLY, O_RDWR... */
#include <errno.h>          /* For errno */
#include <math.h>           /* For pow */
#include <regex.h>
#include <curl/curl.h>
#include <sys/resource.h>

#define true 1
#define false 0

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)

#define MAXLINE 1024
#define SUB_STR_SIZE 10 //正则匹配，子字符串的存储区个数
// below in syslog.h
// #define LOG_EMERG   0   /* system is unusable */
// #define LOG_ALERT   1   /* action must be taken immediately */
// #define LOG_CRIT    2   /* critical conditions */
// #define LOG_ERR     3   /* error conditions */
// #define LOG_WARNING 4   /* warning conditions */
// #define LOG_NOTICE  5   /* normal but significant condition */
// #define LOG_INFO    6   /* informational */
// #define LOG_DEBUG   7   /* debug-level messages */
// #define LOG_LEVEL LOG_INFO
#define LOG_LEVEL LOG_DEBUG

typedef struct DH_ele {
    uint64_t p;
    uint64_t g;
    uint64_t a;
    uint64_t b;
    uint64_t A;
    uint64_t B;
    uint64_t rA;
    uint64_t rB;
} DH_ELE;

extern char LOG_DATE[16];
extern struct tm *DATE_TIME;
extern char LOG_PATH[];
extern char log_buf[BUFSIZ];
extern FILE *LOG_FP;
extern FILE *CUR_LOG_FP;
extern char global_tmp_buf[BUFSIZ*10];
extern const char hex[];
/* 映射出ASCII码中十六进制字符的实际数值 */
extern const int dehex[];
extern const char base64_chars[];
/* 映射出ASCII码中相关字符在base64_chars中的索引值,比如A是base64_chars中的第0个字符 */
extern const uint8_t base64_dechars[];

extern void err_doit(int errnoflag, int error, const char *fmt, va_list ap);
extern void err_sys(const char *fmt, ...);
extern void err_dump(const char *fmt, ...);
extern void err_msg(const char *fmt, ...);
extern void err_ret(const char *fmt, ...);
extern void error(const char *pMsg);
extern void setnonblocking(int32_t fd);
extern char* num2str(const int64_t num);

extern void cur_date_time();
extern void log_time();
extern int logger_init();
extern void clogger(const char *pMsg);
extern void logger(const int level, const char *fmt, ...);
extern char* substr(const char *str, const unsigned int start, const unsigned int end);
extern char* dirname(const char *full_path);
extern char* cbasename(const char *full_path);
extern char* dirnext(const char *full_path, const char *cur_path);
extern int make_path(const char *path);
extern int regex_match(regmatch_t *pm, char *str, char *pattern);
extern size_t WriteFileCallback(void *contents, size_t size, size_t nmemb, void *dst_path);
extern int get_file_to(const char *src_url, const char *dst_path);
extern void get_file_in_list_to(const char *base_url, const char *list_path);
extern void UrlEncode(const char *src, char *dst);
extern void UrlDecode(const char *src, char *dst);
extern void base64_encode(const char *src, char *dst);
extern void base64_decode(const char *src, char *dst);
extern DH_ELE DH_init();
extern void DH_side_A(DH_ELE *dh_ele);
extern void DH_side_B(DH_ELE *dh_ele);
extern int catch_crash();
extern void daemonize(const char *cmd);

#endif
