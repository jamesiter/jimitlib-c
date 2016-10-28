#include "utils.h"

char LOG_DATE[16];
struct timeval TV;
struct tm *DATE_TIME;
char LOG_PATH[] = "/var/log/map-server.log";
char log_buf[BUFSIZ];
FILE *LOG_FP = NULL;
FILE *CUR_LOG_FP = NULL;
char global_tmp_buf[BUFSIZ*10];

const char hex[] = "0123456789ABCDEF";
/* 映射出ASCII码中十六进制字符的实际数值 */
const int dehex[] = {0,0,0,0,0,0,0,0,0,0,0
                            ,0,0,0,0,0,0,0,0,0,0
                            ,0,0,0,0,0,0,0,0,0,0
                            ,0,0,0,0,0,0,0,0,0,0
                            ,0,0,0,0,0,0,0,0,1,2
                            ,3,4,5,6,7,8,9,0,0,0
                            ,0,0,0,0,10,11,12,13,14,15
                            ,0,0,0,0,0,0,0,0,0,0
                            ,0,0,0,0,0,0,0,0,0,0
                            ,0,0,0,0,0,0,10,11,12,13
                            ,14,15};
const char base64_chars[] =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";
/* 映射出ASCII码中相关字符在base64_chars中的索引值,比如A是base64_chars中的第0个字符 */
const uint8_t base64_dechars[] = {0,0,0,0,0,0,0,0,0,0,0          /* +0   */
                                    ,0,0,0,0,0,0,0,0,0,0                /* +10  */
                                    ,0,0,0,0,0,0,0,0,0,0                /* +20  */
                                    ,0,0,0,0,0,0,0,0,0,0                /* +30  */
                                    ,0,0,62,0,0,0,63,52,53,54           /* +40  */
                                    ,55,56,57,58,59,60,61,0,0,0         /* +50  */
                                    ,0,0,0,0,0,1,2,3,4,5                /* +60  */
                                    ,6,7,8,9,10,11,12,13,14,15          /* +70  */
                                    ,16,17,18,19,20,21,22,23,24,25      /* +80  */
                                    ,0,0,0,0,0,0,26,27,28,29            /* +90  */
                                    ,30,31,32,33,34,35,36,37,38,39      /* +100 */
                                    ,40,41,42,43,44,45,46,47,48,49      /* +110 */
                                    ,50,51};                            /* +120 */

void err_doit(int errnoflag, int error, const char *fmt, va_list ap) {
    char buf[MAXLINE];
    vsnprintf(buf, MAXLINE, fmt, ap);
    if (errnoflag) {
        snprintf(buf+strlen(buf), MAXLINE-strlen(buf), ": %s", strerror(error));
    }
    strcat(buf, "\n");  // Make new line;
//     fflush(stdout);     // In case stdout and stderr are the same;
    fputs(buf, stderr);
    fflush(NULL);       // Flushes all stdio output streams;
}

void err_sys(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(true, errno, fmt, ap);
    va_end(ap);
    exit(1);
}

void err_dump(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(true, errno, fmt, ap);
    va_end(ap);
    abort();
    exit(1);
}

void err_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(false, 0, fmt, ap);
    va_end(ap);
}

void err_ret(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_doit(true, errno, fmt, ap);
    va_end(ap);
}

void error(const char *pMsg) { perror(pMsg); exit(1); }
void setnonblocking(const int fd) {
    int opts = fcntl(fd, F_GETFL);
    if(opts<0) {
        err_sys("ERROR:error by fcntl(fd, F_GETFL)!");
    }
    opts = opts | O_NONBLOCK;
    if(fcntl(fd, F_SETFL, opts) < 0) {
        err_sys("ERROR:error by fcntl(fd, F_SETFL, opts)!");
    }
}

char* num2str(const int64_t num) {
    static char ststr_num[MAXLINE];
    sprintf(ststr_num, "%ld", num);
    return ststr_num;
}

void cur_date_time() {
    gettimeofday(&TV, NULL);
    DATE_TIME = localtime(&TV.tv_sec);
}

/* 更新LOG_DATE时间*/
void log_time() {
    cur_date_time();
    strftime(LOG_DATE, sizeof(LOG_DATE), "%b %d %H:%M:%S", DATE_TIME);
}

/* 日志输出*/
int logger_init() {
    LOG_FP = fopen(LOG_PATH, "a+");
    setvbuf(LOG_FP, NULL, _IOLBF, BUFSIZ);
    CUR_LOG_FP = LOG_FP;
    return 0;
}   

void clogger(const char *pMsg) {
    logger(LOG_INFO, "%s\n", pMsg);
}

void logger(const int level, const char *fmt, ...) {
    if (level <= LOG_LEVEL) {
        log_time();
        fprintf(CUR_LOG_FP, "%s %lu\t", LOG_DATE, TV.tv_usec);
        va_list argptr;
        va_start(argptr, fmt);
        // vprintf与printf类似，不同的是，它用一个参数取代了变长参数表；
        // 若要顺便写到标准输出，开启下面；
        // vprintf( fmt, argptr);
        // 每次重读可变参数，需重置其头；
        // va_start(argptr, fmt);
        vsprintf( log_buf, fmt, argptr);
        fwrite( log_buf, sizeof(char), strlen(log_buf), CUR_LOG_FP);
        va_end(argptr);
    }
}

/* 取子串的函数 */
char* substr(const char *str, const unsigned int start, const unsigned int end) {
    unsigned int n = end - start;
    static char stbuf[MAXLINE];
    strncpy(stbuf, str + start, n); 
    stbuf[n] = 0;
    return stbuf;
}

/* 获取路径的目录名*/
char* dirname(const char *full_path) {
    char dup_path[MAXLINE];
    // replace 'dir/' to '.';
    char cur_dir[] = ".";
    strncpy(dup_path, full_path, sizeof(dup_path));
    char *seek_ptr = dup_path + strlen(dup_path);
    // Drop /dir'/////';
    do { // First skip tail char '\0' of string;
        *seek_ptr = '\0';
        seek_ptr--;
    } while('/' == *seek_ptr);
    seek_ptr = strrchr(dup_path, '/');
    if (NULL == seek_ptr) {
        seek_ptr = dup_path;
    }
    // Drop /dir/'//'dir;
    while('/' == *(seek_ptr-1)) {
        seek_ptr--;
    }
    int end_point = (seek_ptr - dup_path);
    if (0 == end_point) {
        if ('/' != dup_path[end_point]) {
            return substr(cur_dir, 0, strlen(cur_dir));
        }
        end_point = 1;
    }
    return substr(dup_path, 0, end_point);
}

/* 获取路径的文件名*/
char* cbasename(const char *full_path) {
    int begin_point = (strrchr(full_path, '/') - full_path);
    return substr(full_path, ++begin_point, strlen(full_path));
}

/* 在full_path中，获取cur_path的下一个目录路径 */
char* dirnext(const char *full_path, const char *cur_path) {
    char *seek_ptr = strchr((char *) full_path + strlen(cur_path)+1, '/');
    if (NULL == seek_ptr) {
        return substr(full_path, 0, strlen(full_path));
    } else {
        int next_point = (seek_ptr - full_path);
        return substr(full_path, 0, next_point);
    }
}

int make_path(const char *path) {
    if (0 == access(path, F_OK)) {
        err_ret("ERROR:%s exist!", path);
        return false;
    }   
    char *depth_ptr = NULL;
    char *cur_path = dirname(path);
    while(true) {
        /* If the parent dir of cur_path is not exist,
         * so, keep on get its parent until exist! */
        if (0 > access(cur_path, F_OK)) {
            cur_path = dirname(cur_path);
        } else if (0 > access(cur_path, W_OK)) { /* If can't write! */
            err_ret("ERROR:access error for %s", cur_path);
            return false;
        } else { /* cur_path exist and can write! */
            cur_path = dirnext(path, cur_path);
            if(0 != mkdir(cur_path, DIR_MODE)) {
                err_ret("ERROR:error by mkdir %s", cur_path);
                return false;
            }
            if (strlen(path) == strlen(cur_path)) {
                return true;
            }
        }
    }
}

/* 正则匹配封装函数*/
int regex_match(regmatch_t *pm, char *str, char *pattern) {
    /*
     * 使用示例；
     *  regmatch_t pm[SUB_STR_SIZE];
     *  if(regex_match((regmatch_t *)&pm, line_buf, pattern)) {
     *    for (int x = 1; x < SUB_STR_SIZE && pm[x].rm_so != -1; ++x) {
     *      logger(LOG_INFO, "%s\n", substr(line_buf, pm[x].rm_so, pm[x].rm_eo));
     *    }
     *  }
     */
    regex_t reg;
    const size_t nmatch = SUB_STR_SIZE;
    char err_buf[MAXLINE];
    int z, cflags = 0, rflag = 1;
    z = regcomp(&reg, pattern, cflags);
    if (z != 0) {
        regerror(z, &reg, err_buf, sizeof(err_buf));
        fprintf(stderr, "%s: pattern '%s' \n", err_buf, pattern);
        rflag = 0;
    }
    z = regexec(&reg, str, nmatch, pm, 0);
    if (z == REG_NOMATCH) {
        rflag = 0;
    } else if (z != 0) {
        regerror(z, &reg, err_buf, sizeof(err_buf));
        fprintf(stderr, "%s: regcom('%s')\n", err_buf, str);
        rflag = 0;
    }
    regfree(&reg);
    return rflag;
}

/* 把curl获取的数据写入文件中*/
size_t WriteFileCallback(void *contents, size_t size, size_t nmemb, void *dst_path)
{
    size_t realsize = size * nmemb;
    FILE *fp = fopen((char *) dst_path, "a");
    fwrite(contents, 1, realsize, fp);
    fclose(fp);
    return realsize;
}

/* 获取internet上的文件到本地*/
int get_file_to(const char *src_url, const char *dst_path) {
    // Reference: http://curl.haxx.se/libcurl/c/getinmemory.html;
    unlink(dst_path);
    printf("src_url: %s\n", src_url);
    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, src_url);
    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) dst_path);
    /* some servers don't like requests that are made without a user-agent
     * field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    /* get it! */
    res = curl_easy_perform(curl_handle);
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();
    /* check for errors */
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return false;
    }
    else {
        return true;
    }
}

void get_file_in_list_to(const char *base_url, const char *list_path) {
    struct stat sb;
    char line_buf[MAXLINE];
    char file_url[MAXLINE];
    char file_dst[MAXLINE];
    FILE *fp;
    fp = fopen(list_path, "r");
    while(NULL != fgets(line_buf, MAXLINE, fp)) {
        *strrchr(line_buf, '\n') = '\0';
        strncpy(file_url, base_url, sizeof(file_url));
        strncpy(file_dst, dirname(list_path), sizeof(file_dst));
        strcat(file_url, "/");
        strcat(file_url, line_buf);
        strcat(file_dst, "/");
        strcat(file_dst, line_buf);
        get_file_to(file_url, file_dst);
    }
    fclose(fp);
}

// Abide by RFC 3986
void UrlEncode(const char *src, char *dst) {
    int i = 0;
    uint8_t c;
    char *proc_ptr = dst;
    // About ascii and unicode: http://www.ruanyifeng.com/blog/2007/10/ascii_unicode_and_utf-8.html
    // Reference: http://zh.wikipedia.org/wiki/Urlencode#.E5.BD.93.E5.89.8D.E6.A0.87.E5.87.86
    for (; i < strlen(src); i++) {
        c = src[i];
        // In addition to these [A-Za-z0-9-_.~];
        if (45 == c || 46 == c || 95 == c || 126 == c
            || (48 <= c && 57 >= c) || (65 <= c && 90 >= c)
            || (97 <= c && 122 >=c) ) {
            *proc_ptr = c;
            proc_ptr++;
            continue;
        }
        *proc_ptr = '%';
        proc_ptr++;
        *proc_ptr = hex[c / 16];
        proc_ptr++;
        *proc_ptr = hex[c % 16];
        proc_ptr++;
    }
    *proc_ptr = '\0';
}

void UrlDecode(const char *src, char *dst) {
    int i = 0;
    char *proc_ptr = dst;
    for (; i < strlen(src); i++) {
        if ('%' == src[i]) {
            *proc_ptr = dehex[src[++i]] * 16 + dehex[src[++i]];
        } else {
            *proc_ptr = src[i];
        }
        proc_ptr++;
    }
    *proc_ptr = '\0';
}

void base64_encode(const char *src, char *dst) {
    // Reference: http://zh.wikipedia.org/wiki/Base64#.E4.BE.8B.E5.AD.90
    uint8_t bitand_frist = 252;         // 0x11111100;
    uint8_t bitand_second_high = 3;     // 0x00000011;
    uint8_t bitand_second_low = 240;    // 0x11110000;
    uint8_t bitand_third_high = 15;     // 0x00001111;
    uint8_t bitand_third_low = 192;     // 0x11000000;
    uint8_t bitand_fourth = 63;         // 0x00111111;
    char *proc_ptr = dst;
    int i = 0, n = 0, srclen, q3, r3;
    srclen = strlen(src);
    q3 = srclen / 3;    // quotient 3;
    r3 = srclen % 3;    // remainder 3;
    while(n < (q3 * 4)) {
        n+=4;
        *proc_ptr = base64_chars[(bitand_frist & src[i]) / 4]; // 4 = 2^2;
        proc_ptr++;
        *proc_ptr = base64_chars[((bitand_second_high & src[i]) * 16) +
                    ((bitand_second_low & src[++i]) / 16 )]; // 16 = 2^4;
        proc_ptr++;
        *proc_ptr = base64_chars[((bitand_third_high & src[i]) * 4) +
            ((bitand_third_low & src[++i]) / 64)]; // 4 = 2^2, 64 = 2^6;
        proc_ptr++;
        *proc_ptr = base64_chars[bitand_fourth & src[i]];
        proc_ptr++;
        i++;
    }
    if (0 != r3) {
        *proc_ptr = base64_chars[(bitand_frist & src[i]) / 4]; // 4 = 2^2;
        proc_ptr++;
        *proc_ptr = base64_chars[((bitand_second_high & src[i]) * 16) +
                    ((bitand_second_low & src[++i]) / 16 )]; // 16 = 2^4;
        proc_ptr++;
        *proc_ptr = '=';
        proc_ptr++;
        if (2 == r3) {
            proc_ptr--;
            *proc_ptr = base64_chars[((bitand_third_high & src[i]) * 4) +
                ((bitand_third_low & src[++i]) / 64)]; // 4 = 2^2, 64 = 2^6;
            proc_ptr++;
            i++;
        }
        *proc_ptr = '=';
        proc_ptr++;
    }
    *proc_ptr = '\0';
}

void base64_decode(const char *src, char *dst) {
    int i = 0, n = 0, srclen, q4, r4;
    char *proc_ptr = dst;
    srclen = strlen(src);
    q4 = srclen / 4;    // quotient 4;
    if ('=' == src[srclen-1]) {
        q4--;
        r4 = 1;
        if ('=' == src[srclen-2]) {
            r4 = 2;
        }
    }
    while(n < q4) {
        n++;
        /* 运算所得值，均不会超过122 */
        *proc_ptr = base64_dechars[src[i]] * 4 +  base64_dechars[src[++i]] / 16;
        proc_ptr++;
        *proc_ptr = base64_dechars[src[i]] % 16 * 16 +  base64_dechars[src[++i]] / 4;
        proc_ptr++;
        *proc_ptr = base64_dechars[src[i]] % 4 * 64 +  base64_dechars[src[++i]];
        proc_ptr++;
        i++;
    }
    if (0 != r4) {
        *proc_ptr = base64_dechars[src[i]] * 4 +  base64_dechars[src[++i]] / 16;
        proc_ptr++;
        if (1 == r4) {
            *proc_ptr = base64_dechars[src[i]] % 16 * 16 +  base64_dechars[src[++i]] / 4;
            proc_ptr++;
        }
    }
    *proc_ptr = '\0';
}

DH_ELE DH_init() {
    srandom(time(NULL));
    DH_ELE dh_ele;
    dh_ele.p = random() % 40;
    dh_ele.g = 2;
    dh_ele.a = 0;
    dh_ele.b = 0;
    dh_ele.A = 0;
    dh_ele.B = 0;
    dh_ele.rA = 0;
    dh_ele.rB = 0;
    return dh_ele;
}

void DH_side_A(DH_ELE *dh_ele) {
    dh_ele->a = random() % 20;
    // dh_ele->A = pow(dh_ele->g, dh_ele->a) % dh_ele->p;
    dh_ele->A = pow(dh_ele->g, dh_ele->a);
    dh_ele->A %= dh_ele->p;
}

void DH_side_B(DH_ELE *dh_ele) {
    dh_ele->b = random() % 20;
    // dh_ele->B = pow(dh_ele->g, dh_ele->b) % dh_ele->p;
    dh_ele->B = pow(dh_ele->g, dh_ele->b);
    dh_ele->B %= dh_ele->p;
}

void DH_get_secret(DH_ELE *dh_ele) {
    dh_ele->rA = pow(dh_ele->B, dh_ele->a);
    dh_ele->rA %= dh_ele->p;
    dh_ele->rB = pow(dh_ele->A, dh_ele->b);
    dh_ele->rB %= dh_ele->p;
}

int catch_crash() {
    struct rlimit rlim = {RLIM_INFINITY,RLIM_INFINITY};
    if (0 != setrlimit(RLIMIT_CORE, &rlim))
    {
    }
    #ifdef __linux__
    int fp = open("/proc/sys/kernel/core_pattern", O_WRONLY);
    if(fp < 0)
    {
        return 1;
    }
    const char * const str_core_pattern = "core.%e.%p";
    if(write(fp, str_core_pattern, strlen(str_core_pattern)) < 0)
    {
        close(fp);
        return 1;
    }
    close(fp);
    #endif
}

void daemonize(const char *cmd) {
    int i, fd0, fd1, fd2;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;
    umask(0);
    if (0 > getrlimit( RLIMIT_NOFILE, &rl)) {
        printf("%s: can't get file limit\n", cmd); _exit(-1);
    }
    if (0 > (pid = fork())) {
        printf("%s: can't fork", cmd); _exit(-1);
    } else if (0 != pid) {
        exit(0);
    }
    setsid();
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (0 > sigaction( SIGHUP, &sa, NULL)) {
        printf("%s: can't ignore SIGHUP", cmd); _exit(-1);
    }
    if (0 > (pid = fork())) {
        printf("%s: can't fork", cmd); _exit(-1);
    } else if (0 != pid) {
        exit(0);
    }
//    if (0 > chdir("/")) {
//        printf("%s: can't change directory to /", cmd); _exit(-1);
//    }
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (i = 0; i < rl.rlim_max; i++) {
        close(i);
    }
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);
}
