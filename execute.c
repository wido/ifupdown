#line 1980 "ifupdown.nw"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "header.h"
#line 2577 "ifupdown.nw"
#include <errno.h>
#line 2763 "ifupdown.nw"
#include <stdarg.h>
#line 2786 "ifupdown.nw"
#include <unistd.h>
#include <sys/wait.h>
#line 2064 "ifupdown.nw"
static char **environ = NULL;
#line 2041 "ifupdown.nw"
static int check(char *str);
#line 2071 "ifupdown.nw"
static void set_environ(interface_defn *iface, char *mode);
#line 2147 "ifupdown.nw"
static char *setlocalenv(char *format, char *name, char *value);
#line 2245 "ifupdown.nw"
static int doit(char *str);
#line 2404 "ifupdown.nw"
static char *parse(char *command, interface_defn *ifd);
#line 2451 "ifupdown.nw"
void addstr(char **buf, size_t *len, size_t *pos, char *str, size_t strlen);
#line 2658 "ifupdown.nw"
int strncmpz(char *l, char *r, size_t llen);
#line 2675 "ifupdown.nw"
char *get_var(char *id, size_t idlen, interface_defn *ifd);
#line 2767 "ifupdown.nw"
static int popen2(FILE **in, FILE **out, char *command, ...);
#line 2045 "ifupdown.nw"
static int check(char *str) {
	return str != NULL;
}
#line 2077 "ifupdown.nw"
static void set_environ(interface_defn *iface, char *mode) {
	
#line 2102 "ifupdown.nw"
char **environend;
#line 2079 "ifupdown.nw"
	int i;
	const int n_env_entries = iface->n_options + 5;

	
#line 2115 "ifupdown.nw"
{
	char **ppch;
	if (environ != NULL) {
		for (ppch = environ; *ppch; ppch++) {
			free(*ppch);
			*ppch = NULL;
		}
		free(environ);
		environ = NULL;
	}
}
#line 2109 "ifupdown.nw"
environ = malloc(sizeof(char*) * (n_env_entries + 1 /* for final NULL */));
environend = environ; 
*environend = NULL;

#line 2084 "ifupdown.nw"
	for (i = 0; i < iface->n_options; i++) {
		
#line 2131 "ifupdown.nw"
if (strcmp(iface->option[i].name, "up") == 0
    || strcmp(iface->option[i].name, "down") == 0
    || strcmp(iface->option[i].name, "pre-up") == 0
    || strcmp(iface->option[i].name, "post-down") == 0)
{
	continue;
}

#line 2087 "ifupdown.nw"
		
#line 2153 "ifupdown.nw"
*(environend++) = setlocalenv("IF_%s=%s", iface->option[i].name,
                              iface->option[i].value);
*environend = NULL;
#line 2088 "ifupdown.nw"
	}

	
#line 2159 "ifupdown.nw"
*(environend++) = setlocalenv("%s=%s", "IFACE", iface->iface);
*environend = NULL;
#line 2091 "ifupdown.nw"
	
#line 2174 "ifupdown.nw"
*(environend++) = setlocalenv("%s=%s", "ADDRFAM", iface->address_family->name);
*environend = NULL;
#line 2092 "ifupdown.nw"
	
#line 2179 "ifupdown.nw"
*(environend++) = setlocalenv("%s=%s", "METHOD", iface->method->name);
*environend = NULL;
#line 2093 "ifupdown.nw"
	
#line 2164 "ifupdown.nw"
*(environend++) = setlocalenv("%s=%s", "MODE", mode);
*environend = NULL;
#line 2094 "ifupdown.nw"
	
#line 2169 "ifupdown.nw"
*(environend++) = setlocalenv("%s=%s", "PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
*environend = NULL;
#line 2095 "ifupdown.nw"
}
#line 2186 "ifupdown.nw"
static char *setlocalenv(char *format, char *name, char *value) {
	char *result;

	
#line 2204 "ifupdown.nw"
result = malloc(strlen(format)   /* -4 for the two %s's */
                + strlen(name) 
                + strlen(value) 
                + 1);
if (!result) {
	perror("malloc");
	exit(1);
}

#line 2191 "ifupdown.nw"
	sprintf(result, format, name, value);

	
#line 2220 "ifupdown.nw"
{
	char *here, *there;

	for(here = there = result; *there != '=' && *there; there++) {
		if (*there == '-') *there = '_';
		if (isalpha(*there)) *there = toupper(*there);

		if (isalnum(*there) || *there == '_') {
			*here = *there;
			here++;
		}
	}
	memmove(here, there, strlen(there) + 1);
}

#line 2195 "ifupdown.nw"
	return result;
}
#line 2249 "ifupdown.nw"
static int doit(char *str) {
	assert(str);

	if (verbose || no_act) {
		fprintf(stderr, "%s\n", str);
	}
	if (!no_act) {
		pid_t child;
		int status;

		fflush(NULL);
		switch(child = fork()) {
		    case -1: /* failure */
			return 0;
		    case 0: /* child */
			execle("/bin/sh", "/bin/sh", "-c", str, NULL, environ);
			exit(127);
		    default: /* parent */
		    	break;
		}
		waitpid(child, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			return 0;
	}
	return 1;
}
#line 2295 "ifupdown.nw"
int execute_all(interface_defn *ifd, execfn *exec, char *opt) {
	int i;
	char buf[100];
	for (i = 0; i < ifd->n_options; i++) {
		if (strcmp(ifd->option[i].name, opt) == 0) {
			if (!(*exec)(ifd->option[i].value)) {
				return 0;
			}
		}
	}

	sprintf(buf, "run-parts /etc/network/if-%s.d", opt);
	(*exec)(buf); 

	return 1;
}
#line 2323 "ifupdown.nw"
int iface_up(interface_defn *iface) {
	if (!iface->method->up(iface,check)) return -1;

	set_environ(iface, "start");
	if (!execute_all(iface,doit,"pre-up")) return 0;
	if (!iface->method->up(iface,doit)) return 0;
	if (!execute_all(iface,doit,"up")) return 0;

	return 1;
}
#line 2336 "ifupdown.nw"
int iface_down(interface_defn *iface) {
	if (!iface->method->down(iface,check)) return -1;

	set_environ(iface, "stop");
	if (!execute_all(iface,doit,"down")) return 0;
	if (!iface->method->down(iface,doit)) return 0;
	if (!execute_all(iface,doit,"post-down")) return 0;

	return 1;
}
#line 2362 "ifupdown.nw"
int execute(char *command, interface_defn *ifd, execfn *exec) { 
	char *out;
	int ret;

	out = parse(command, ifd);
	if (!out) { return 0; }

	ret = (*exec)(out);

	free(out);
	return ret;
}
#line 2408 "ifupdown.nw"
static char *parse(char *command, interface_defn *ifd) {
	
#line 2433 "ifupdown.nw"
char *result = NULL;
size_t pos = 0, len = 0;
#line 2527 "ifupdown.nw"
size_t old_pos[MAX_OPT_DEPTH] = {0};
int okay[MAX_OPT_DEPTH] = {1};
int opt_depth = 1;

#line 2411 "ifupdown.nw"
	while(*command) {
		switch(*command) {
			
#line 2481 "ifupdown.nw"
default:
	addstr(&result, &len, &pos, command, 1);
	command++;
	break;
#line 2494 "ifupdown.nw"
case '\\':
	if (command[1]) {
		addstr(&result, &len, &pos, command+1, 1);
		command += 2;
	} else {
		addstr(&result, &len, &pos, command, 1);
		command++;
	}
	break;
#line 2542 "ifupdown.nw"
case '[':
	if (command[1] == '[' && opt_depth < MAX_OPT_DEPTH) {
		old_pos[opt_depth] = pos;
		okay[opt_depth] = 1;
		opt_depth++;
		command += 2;
	} else {
		addstr(&result, &len, &pos, "[", 1);
		command++;
	}
	break;
#line 2556 "ifupdown.nw"
case ']':
	if (command[1] == ']' && opt_depth > 1) {
		opt_depth--;
		if (!okay[opt_depth]) {
			pos = old_pos[opt_depth];
			result[pos] = '\0';
		}
		command += 2;
	} else {
		addstr(&result, &len, &pos, "]", 1);
		command++;
	}
	break;
#line 2611 "ifupdown.nw"
case '%':
{
	
#line 2636 "ifupdown.nw"
char *nextpercent;
#line 2614 "ifupdown.nw"
	char *varvalue;

	
#line 2640 "ifupdown.nw"
command++;
nextpercent = strchr(command, '%');
if (!nextpercent) {
	errno = EUNBALPER;
	free(result);
	return NULL;
}

#line 2618 "ifupdown.nw"
	
#line 2699 "ifupdown.nw"
varvalue = get_var(command, nextpercent - command, ifd);

#line 2620 "ifupdown.nw"
	if (varvalue) {
		addstr(&result, &len, &pos, varvalue, strlen(varvalue));
	} else {
		okay[opt_depth - 1] = 0;
	}

	
#line 2650 "ifupdown.nw"
command = nextpercent + 1;

#line 2628 "ifupdown.nw"
	break;
}
#line 2414 "ifupdown.nw"
		}
	}

	
#line 2586 "ifupdown.nw"
if (opt_depth > 1) {
	errno = EUNBALBRACK;
	free(result);
	return NULL;
}

if (!okay[0]) {
	errno = EUNDEFVAR;
	free(result);
	return NULL;
}

#line 2419 "ifupdown.nw"
	
#line 2440 "ifupdown.nw"
return result;
#line 2420 "ifupdown.nw"
}
#line 2455 "ifupdown.nw"
void addstr(char **buf, size_t *len, size_t *pos, char *str, size_t strlen) {
	assert(*len >= *pos);
	assert(*len == 0 || (*buf)[*pos] == '\0');

	if (*pos + strlen >= *len) {
		char *newbuf;
		newbuf = realloc(*buf, *len * 2 + strlen + 1);
		if (!newbuf) {
			perror("realloc");
			exit(1); /* a little ugly */
		}
		*buf = newbuf;
		*len = *len * 2 + strlen + 1;
	}

	while (strlen-- >= 1) {
		(*buf)[(*pos)++] = *str;
		str++;
	}
	(*buf)[*pos] = '\0';
}
#line 2662 "ifupdown.nw"
int strncmpz(char *l, char *r, size_t llen) {
	int i = strncmp(l, r, llen);
	if (i == 0)
		return -r[llen];
	else
		return i;
}
#line 2679 "ifupdown.nw"
char *get_var(char *id, size_t idlen, interface_defn *ifd) {
	int i;

	if (strncmpz(id, "iface", idlen) == 0) {
		return ifd->iface;
	} else {
		for (i = 0; i < ifd->n_options; i++) {
			if (strncmpz(id, ifd->option[i].name, idlen) == 0) {
				return ifd->option[i].value;
			}
		}
	}

	return NULL;
}
#line 2718 "ifupdown.nw"
int run_mapping(char *physical, char *logical, int len, mapping_defn *map) {
	FILE *in, *out;
	int i, status;
	pid_t pid;

	
#line 2777 "ifupdown.nw"
pid = popen2(&in, &out, map->script, physical, NULL);
if (pid == 0) {
	return 0;
}
#line 2724 "ifupdown.nw"
	
#line 2736 "ifupdown.nw"
for (i = 0; i < map->n_mappings; i++) {
	fprintf(in, "%s\n", map->mapping[i]);
}
fclose(in);
#line 2725 "ifupdown.nw"
	
#line 2743 "ifupdown.nw"
waitpid(pid, &status, 0);
#line 2726 "ifupdown.nw"
	
#line 2747 "ifupdown.nw"
if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
	if (fgets(logical, len, out)) {
		char *pch = logical + strlen(logical) - 1;
		while (pch >= logical && isspace(*pch)) 
			*(pch--) = '\0';
	}
}
fclose(out);	

#line 2728 "ifupdown.nw"
	return 1;
}
#line 2791 "ifupdown.nw"
static int popen2(FILE **in, FILE **out, char *command, ...) {
	va_list ap;
	char *argv[11] = {command};
	int argc;
	int infd[2], outfd[2];
	pid_t pid;

	argc = 1;
	va_start(ap, command);
	while((argc < 10) && (argv[argc] = va_arg(ap, char*))) {
		argc++;
	}
	argv[argc] = NULL; /* make sure */
	va_end(ap);

	if (pipe(infd) != 0) return 0;
	if (pipe(outfd) != 0) {
		close(infd[0]); close(infd[1]);
		return 0;
	}

	fflush(NULL);
	switch(pid = fork()) {
		case -1: /* failure */
			close(infd[0]); close(infd[1]);
			close(outfd[0]); close(outfd[1]);
			return 0;
		case 0: /* child */
			dup2(infd[0], 0);
			dup2(outfd[1], 1);
			close(infd[0]); close(infd[1]);
			close(outfd[0]); close(outfd[1]);
			execvp(command, argv);
			exit(127);
		default: /* parent */
			*in = fdopen(infd[1], "w");
			*out = fdopen(outfd[0], "r");
			close(infd[0]);	close(outfd[1]);
			return pid;
	}
	/* unreached */
}
