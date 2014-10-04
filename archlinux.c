#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include "archlinux.h"

unsigned int mylinuxver(void)
{
    static int maj = -1, rev = 0, min = 0;

    if (maj == -1) {
        struct utsname u;
        char *pch;
        uname(&u);
        maj = atoi(u.release);
        pch = strchr(u.release, '.');
        if (pch) {
            rev = atoi(pch + 1);
            pch = strchr(pch + 1, '.');
            if (pch) {
                min = atoi(pch + 1);
            }
        }
    }

    return mylinux(maj, rev, min);
}

unsigned int mylinux(int maj, int rev, int min)
{
    return min | rev << 10 | maj << 13;
}

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

int _iface_has(char *iface, char *delims)
{
    char _iface[80];
    strncpy(_iface, iface, sizeof(_iface));
    _iface[sizeof(_iface) - 1] = 0;
    strtok(_iface, delims);
    void *token = strtok(NULL, delims);
    return (token != NULL);
}

int execable(char *program)
{
    struct stat buf;

    if (0 == stat(program, &buf)) {
        if (S_ISREG(buf.st_mode) && (S_IXUSR & buf.st_mode))
            return 1;
    }
    return 0;
}

void cleanup_hwaddress(interface_defn * ifd UNUSED, char **pparam, int argc UNUSED, char **argv UNUSED)
{
    char *rest = *pparam;
    /* we're shrinking the text, so no realloc needed */
    char *space = strchr(rest, ' ');

    if (space == NULL)
        return;

    *space = '\0';
    if (strcasecmp(rest, "ether") == 0 || strcasecmp(rest, "ax25") == 0 || strcasecmp(rest, "ARCnet") == 0 || strcasecmp(rest, "netrom") == 0) {
        /* found deprecated <class> attribute */
        memmove(rest, space + 1, strlen(space + 1) + 1);
    } else {
        *space = ' ';
    }
}

void make_hex_address(interface_defn * ifd UNUSED, char **pparam, int argc UNUSED, char **argv UNUSED)
{
    char addrcomp[4];
    int maxlen = strlen("0000:0000");

    int ret = sscanf(*pparam, "%3hhu.%3hhu.%3hhu.%3hhu",
                     &addrcomp[0], &addrcomp[1], &addrcomp[2], &addrcomp[3]);

    if (ret != 4)
        return;

    *pparam = realloc(*pparam, maxlen + 1);
    if (*pparam == NULL)
        return;
    snprintf(*pparam, maxlen + 1, "%.2hhx%.2hhx:%.2hhx%.2hhx", addrcomp[0], addrcomp[1], addrcomp[2], addrcomp[3]);
}

#include <arpa/inet.h>

void compute_v4_addr(interface_defn * ifd UNUSED, char **pparam, int argc UNUSED, char **argv UNUSED)
{
    char s[INET_ADDRSTRLEN * 2 + 2];    /* 2 is for slash and \0 */
    strncpy(s, *pparam, sizeof(s));
    s[sizeof(s) - 1] = 0;

    char *token = strtok(s, "/");
    if (!token)
        return;

    *pparam = realloc(*pparam, strlen(token) + 1);
    if (*pparam == NULL)
        return;
    strcpy(*pparam, token);
}

void compute_v4_mask(interface_defn * ifd UNUSED, char **pparam, int argc UNUSED, char **argv UNUSED)
{
    char s[INET_ADDRSTRLEN * 2 + 2];    /* 2 is for slash and \0 */
    strncpy(s, *pparam, sizeof(s));
    s[sizeof(s) - 1] = 0;

    char *token = strtok(s, "/");
    if (!token)
        return;

    uint8_t addr[sizeof(struct in_addr)];
    struct in_addr mask;
    if (inet_pton(AF_INET, token, &addr) != 1)
        return;
    token = strtok(NULL, "/");
    int maskwidth = -1;
    if (!token) {
        if (addr[0] <= 127) {
            maskwidth = 8;
        } else if ((addr[0] >= 128) && (addr[0] <= 191)) {
            maskwidth = 16;
        } else if ((addr[0] >= 192) && (addr[0] <= 223)) {
            maskwidth = 24;
        } else {
            maskwidth = 32;
        }
    } else {
        switch (inet_pton(AF_INET, token, &mask)) {
            case -1:
                return;

            case 0:
                if (sscanf(token, "%d", &maskwidth) != 1)
                    return;
        }
    }
    if (maskwidth != -1) {
        mask.s_addr = htonl(~((1L << (32 - maskwidth)) - 1));
    }

    if (inet_ntop(AF_INET, &mask, s, sizeof(s)) == NULL)
        return;
    *pparam = realloc(*pparam, strlen(s) + 1);
    if (*pparam == NULL)
        return;
    strcpy(*pparam, s);
}

void compute_v4_broadcast(interface_defn * ifd, char **pparam, int argc UNUSED, char **argv UNUSED)
{
    /* If we don't get special value don't do anything */
    if (strcmp(*pparam, "+") && strcmp(*pparam, "-"))
        return;

    struct in_addr addr;
    struct in_addr mask;

    char *s = get_var("address", strlen("address"), ifd);
    if (!s)
        return;
    int r = inet_pton(AF_INET, s, &addr);
    free(s);
    if (r != 1)
        return;

    s = get_var("netmask", strlen("netmask"), ifd);
    if (!s)
        return;
    r = inet_pton(AF_INET, s, &mask);
    free(s);
    if (r != 1)
        return;

    if (mask.s_addr != htonl(0xfffffffe)) {
        if (!strcmp(*pparam, "+")) {
            addr.s_addr |= ~mask.s_addr;
        }

        if (!strcmp(*pparam, "-")) {
            addr.s_addr &= mask.s_addr;
        }
    } else {
        if (!strcmp(*pparam, "+")) {
            addr.s_addr = 0xffffffff;
        }

        if (!strcmp(*pparam, "-")) {
            addr.s_addr = 0;
        }
    }

    char buffer[INET_ADDRSTRLEN + 1];
    if (inet_ntop(AF_INET, &addr, buffer, sizeof(buffer)) == NULL)
        return;
    *pparam = realloc(*pparam, strlen(buffer) + 1);
    if (*pparam == NULL)
        return;
    strcpy(*pparam, buffer);
}

void set_preferred_lft(interface_defn * ifd, char **pparam, int argc UNUSED, char **argv UNUSED)
{
    if (!ifd->real_iface)
        return;
    if (iface_has(":")) {
        char s[] = "0";
        *pparam = realloc(*pparam, sizeof(s));
        if (*pparam == NULL)
            return;
        strcpy(*pparam, s);
    }
}

void get_token(interface_defn * ifd UNUSED, char **pparam, int argc, char **argv)
{
    if (argc == 0)
        return;

    int token_no;
    if (argc == 1) {
        token_no = 0;
    } else {
        token_no = atoi(argv[1]);
    }

    char *s = strdup(*pparam);
    char *token = strtok(s, argv[0]);
    while (token_no > 0) {
        token = strtok(NULL, argv[0]);
        token_no--;
    }
    if (token) {
        strcpy(*pparam, token);
    } else {
        if (argc == 3) {
            *pparam = realloc(*pparam, strlen(argv[2]) + 1);
            if (*pparam == NULL)
                return;
            strcpy(*pparam, argv[2]);
        }
    }
    free(s);
}

void to_decimal(interface_defn * ifd UNUSED, char **pparam, int argc, char **argv)
{
    int base = 10;

    if (argc == 1) {
        base = atoi(argv[0]);
    }

    char *result;
    long value = strtol(*pparam, &result, base);
    if (result == *pparam)
        return;

    snprintf(*pparam, strlen(*pparam) + 1, "%ld", value);
}

void map_value(interface_defn * ifd UNUSED, char **pparam, int argc, char **argv)
{
    if (argc < 2)
        return;

    int value;
    if (argc == 2) {
        value = (atoi(*pparam) || strcasecmp(*pparam, "on") == 0 || strcasecmp(*pparam, "true") == 0 || strcasecmp(*pparam, "yes") == 0);
    }
    if ((value < argc) && (argv[value] != NULL)) {
        *pparam = realloc(*pparam, strlen(argv[value]) + 1);
        if (*pparam == NULL)
            return;
        strcpy(*pparam, argv[value]);
    } else {
        *pparam = realloc(*pparam, 1);
        if (*pparam == NULL)
            return;
        *pparam[0] = 0;
    }
}
