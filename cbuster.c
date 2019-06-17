/*
 *
 * C directory buster
 * 
 * $ cc cbuster.c -ldill -o cbuster
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <libdill.h>

void usage(char *argument) {
    fprintf(stderr, "Usage: %s -u <url> -w <wordlist> -x <extensions>\n", argument);
    fprintf(stderr, "       -u          URL\n");
    fprintf(stderr, "       -w          Wordlist\n");
    fprintf(stderr, "       -x          Extensions (optional)\n");
    fprintf(stderr, "       -d          Add headers to request (optional)\n");
    fprintf(stderr, "       -t          Threads (default: 10)\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "  %s -u http://example.com:8000/test/ -w rockyou.txt -x php,txt,html -d \"Authorization: YWRtaW46YWRtaW4=\" -t 60\n", argument);
    exit(EXIT_FAILURE);
}

coroutine void prepare_request(char *program_name, char *tempStr, int post_len, int nread, char *cur_dir_req, char *url_postfix, char *url_addr, char *headers, struct ipaddr *res, bool https) {
    int request_length;
    char *get_request;
    char request[4096];
    const char *comma_delim = ",";
    char *cur_ext = strtok(tempStr, comma_delim);
    while (cur_ext != NULL) {
        int newLen = (post_len-1)+(int)nread+(sizeof(cur_ext));
        char dir_req[newLen];
        char tmp_curDir[nread];
        snprintf(tmp_curDir, nread, "%s", cur_dir_req);
        char *tmpPtr = (char *)(&tmp_curDir+1) - 2;
        tmpPtr = '\0';
        tmpPtr = (char *)(&dir_req+1) - 1;
        if (*cur_ext == ' ') {
            snprintf(dir_req, newLen, "%s%s/", url_postfix, tmp_curDir, cur_ext);
        } else {
            snprintf(dir_req, newLen, "%s%s.%s", url_postfix, tmp_curDir, cur_ext);
        }
        tmpPtr = '\0';
        if (headers != "") {
            get_request = "GET %s HTTP/1.0\r\nHost: %s\r\n%s\r\n\r\n";
            request_length = snprintf(request, 4096, get_request, dir_req, url_addr, headers);
        } else {
            get_request = "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n";
            request_length = snprintf(request, 4096, get_request, dir_req, url_addr);
        }
        /* Send the request */
        char buf[1024];
        char *status_code;
        int ret_code, sockfd;
        int check_codes[] = 
            {200,201,202,203,204,205,206,207,208,226,300,301,302,303,304,305,306,307,308,401,402,403,407,418,451};
        sockfd = tcp_connect(res, -1);
        if (sockfd == -1) {
            fprintf(stderr, "[X] Could not connect socket\n");
            exit(EXIT_FAILURE);
        }
        if (https == true) {
            sockfd = tls_attach_client(sockfd, -1);
            if (sockfd == -1) {
                fprintf(stderr, "[X] Error creating TLS protocol on underlying socket\n");
                exit(EXIT_FAILURE);
            }
        }
        bsend(sockfd, request, request_length, -1);
        brecv(sockfd, buf, sizeof(buf), -1);
        if (https == true) {
            sockfd = tls_detach(sockfd, -1);
        }
        tcp_close(sockfd, -1);
        status_code = strchr(buf, ' ');
        status_code++;
        status_code[3] = '\0';
        ret_code = (int)strtol(status_code, NULL, 10);
        for (int i = 0; i < (sizeof(check_codes)/sizeof(check_codes[0])); i++) {
            if (check_codes[i] == ret_code) {
                printf("[%d] %s\n", ret_code, dir_req);
                break;
            }
        }
        /* Request sent */
        //printf("%s", request);
        cur_ext = strtok(NULL, comma_delim);
    }
}

int main(int argc, char **argv) {
    if (argc < 5) { /* Check argument count */
        usage(argv[0]);
    }
    int option = 0;
    int urlf, wlf, xf, df, tf;
    urlf = wlf = xf = df = tf = 0;
    while((option = getopt(argc, argv, "u:w:x:d:t:")) != -1) {
        switch (option) { /* Loop through command line flags */
            case 'u':
                urlf = optind-1;
                break;
            case 'w':
                wlf = optind-1;
                break;
            case 'x':
                xf = optind-1;
                break;
            case 'd':
                df = optind-1;
                break;
            case 't':
                tf = optind-1;
                break;
            default:
                usage(argv[0]);
        }
    }
    if(urlf == 0 || wlf == 0) { /* No url or wordlist != good */
        usage(argv[0]);
    }
    unsigned int port = 80;
    char port_string[6];
    char *url_token;
    char *url_addr;
    char *urlPtr;
    char *headers;
    const char *slash_delim = "/";
    bool https = false;
    url_token = strtok(argv[urlf], slash_delim);
    if (strcmp("http:", url_token) == 0) { /* HTTP Service */
        port = 80;
        url_addr = argv[urlf]+strlen("http://");
    } else if (strcmp("https:", url_token) == 0) { /* HTTPS Service */
        port = 443;
        url_addr = argv[urlf]+strlen("https://");
        https = true;
    }
    urlPtr = strchr(url_addr, *slash_delim); /* Check for directory after domain */
    if (urlPtr == NULL) {
        urlPtr = (char *)slash_delim;
    }
    int post_len = strlen(urlPtr)+1;
    char url_postfix[post_len+1];
    strncpy(url_postfix, urlPtr, post_len);
    char *tmpPtr = &url_postfix[post_len-2];
    if (*tmpPtr != '/') { /* Ensure dir has a / */
        *(++tmpPtr) = '/';
        *(++tmpPtr) = '\0';
    }
    if (urlPtr != slash_delim) {
        urlPtr[0] = '\0';
    }
    urlPtr = strchr(url_addr, ':'); /* Check if port specified after domain */
    if (urlPtr != NULL) {
        port = (unsigned int)strtoul(urlPtr+1, NULL, 10);
        urlPtr[0] = '\0';
    }
    printf("Target: %s//%s:%d\n", url_token, url_addr, port);
    printf("Directory: %s\n", url_postfix);
    if (df != 0) { /* Check if data is appended to request */
        headers = argv[df];
        printf("Data: %s\n", headers);
    } else {
        headers = "";
    }
    int threads;
    if (tf != 0) { /* Check for user defined threads */
        threads = (int)strtol(argv[tf], NULL, 10);
    } else {
        threads = 10;
    }
    printf("Threads: %d\n", threads);
    snprintf(port_string, sizeof(port_string), "%d", port);
    struct ipaddr res;
    int s;
    s = ipaddr_remote(&res, url_addr, port, 0, -1);
    if (s != 0) {
        fprintf(stderr, "[X] Error resolving remote address\n");
        usage(argv[0]);
    }
    char hbuf[17];
    ipaddr_str(&res, hbuf);
    printf("IPv4: %s\n\n", hbuf);
    char *cur_dir_req = NULL;
    FILE *wordlist;
    size_t len = 0;
    ssize_t nread;
    /* Open file, loop through entries and validate */
    wordlist = fopen(argv[wlf], "r");
    if (wordlist == NULL) {
        perror("fopen");
        usage(argv[0]);
    }
    int bees = bundle();
    int val = 0;
    while ((nread = getline(&cur_dir_req, &len, wordlist) ) != -1) {
        if (val == threads) {
            bundle_wait(bees, -1);
            val = 0;
        }
        if (xf != 0) {
            char tempStr[sizeof(argv[xf])+1+2];
            char *tmpPtr = (char *)(&argv[xf]+1) - 1;
            tmpPtr = '\0';
            snprintf(tempStr, sizeof(argv[xf])+3, " ,%s", argv[xf]);
            bundle_go(bees, prepare_request(argv[0], tempStr, post_len, nread, cur_dir_req, url_postfix, url_addr, headers, &res, https));
            val++;
        } else {
            char tempStr[] = " ,";
            bundle_go(bees, prepare_request(argv[0], tempStr, post_len, nread, cur_dir_req, url_postfix, url_addr, headers, &res, https));
            val++;
        }
    }
    bundle_wait(bees, -1);
    hclose(bees);
    free(cur_dir_req);
    fclose(wordlist);

    return 0;
}
