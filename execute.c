#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "header.h"

extern char **environ;
static char **localenv = NULL;

static int check(const char *str) {
	return str != NULL;
}

static char *setlocalenv_nomangle(char *format, char *name, char *value) {
	char *result;

	result = malloc(strlen(format) + strlen(name) + strlen(value) + 1);	/* -4 for the two %s's */
	if (!result) {
		perror("malloc");
		exit(1);
	}

	sprintf(result, format, name, value);

	return result;
}

static char *setlocalenv(char *format, char *name, char *value) {
	char *result;

	result = malloc(strlen(format) + strlen(name) + strlen(value) + 1);	/* -4 for the two %s's */
	if (!result) {
		perror("malloc");
		exit(1);
	}

	sprintf(result, format, name, value);

	char *here, *there;

	for (here = there = result; *there != '=' && *there; there++) {
		if (*there == '-')
			*there = '_';

		if (isalpha(*there))
			*there = toupper(*there);

		if (isalnum(*there) || *there == '_') {
			*here = *there;
			here++;
		}
	}

	memmove(here, there, strlen(there) + 1);

	return result;
}

static void set_environ(interface_defn *iface, char *mode, char *phase) {
	if (localenv != NULL) {
		for (char **ppch = localenv; *ppch; ppch++)
			free(*ppch);

		free(localenv);
	}

	int n_recursion = 0;
	for(char **envp = environ; *envp; envp++)
		if(strncmp(*envp, "IFUPDOWN_", 9) == 0)
			n_recursion++;

	const int n_env_entries = iface->n_options + 10 + n_recursion;
	localenv = malloc(sizeof *localenv * (n_env_entries + 1 /* for final NULL */ ));

	char **ppch = localenv;

	for (int i = 0; i < iface->n_options; i++) {
		if (strcmp(iface->option[i].name, "pre-up") == 0 || strcmp(iface->option[i].name, "up") == 0 || strcmp(iface->option[i].name, "down") == 0 || strcmp(iface->option[i].name, "post-down") == 0)
			continue;

		*ppch++ = setlocalenv("IF_%s=%s", iface->option[i].name, iface->option[i].value ? iface->option[i].value : "");
	}

	for(char **envp = environ; *envp; envp++)
		if(strncmp(*envp, "IFUPDOWN_", 9) == 0)
			*ppch++ = strdup(*envp);

	char piface[80];
	strncpy(piface, iface->real_iface, sizeof piface);
	piface[sizeof piface - 1] = '\0';
	char *pch = strchr(piface, '.');
	if (pch) {
		*pch = '\0';
		*ppch++ = setlocalenv_nomangle("IFUPDOWN_%s=%s", piface, "parent-lock");
	}

	*ppch++ = setlocalenv_nomangle("IFUPDOWN_%s=%s", iface->real_iface, phase);
	*ppch++ = setlocalenv("%s=%s", "IFACE", iface->real_iface);
	*ppch++ = setlocalenv("%s=%s", "LOGICAL", iface->logical_iface);
	*ppch++ = setlocalenv("%s=%s", "ADDRFAM", iface->address_family->name);
	*ppch++ = setlocalenv("%s=%s", "METHOD", iface->method->name);
	*ppch++ = setlocalenv("%s=%s", "MODE", mode);
	*ppch++ = setlocalenv("%s=%s", "PHASE", phase);
	*ppch++ = setlocalenv("%s=%s", "VERBOSITY", verbose ? "1" : "0");
	*ppch++ = setlocalenv("%s=%s", "PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
	*ppch = NULL;
}

int doit(const char *str) {
	assert(str);
	bool ignore_status = false;

	if (*str == '-') {
		ignore_status = true;
		str++;
	}

	if (verbose || no_act)
		fprintf(stderr, "%s\n", str);

	if (!no_act) {
		pid_t child;
		int status;

		fflush(NULL);
		setpgid(0, 0);

		switch (child = fork()) {
		case -1:	/* failure */
			return 0;

		case 0:	/* child */
			execle("/bin/sh", "/bin/sh", "-c", str, NULL, localenv);
			exit(127);

		default:	/* parent */
			break;
		}

		waitpid(child, &status, 0);

		if (ignore_status || ignore_failures)
			return 1;

		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			return 0;
	}

	return 1;
}

static int execute_options(interface_defn *ifd, execfn *exec, char *opt) {
	for (int i = 0; i < ifd->n_options; i++)
		if (strcmp(ifd->option[i].name, opt) == 0)
			if (!(*exec) (ifd->option[i].value))
				if (!ignore_failures)
					return 0;

	return 1;
}

static int execute_scripts(interface_defn *ifd, execfn *exec, char *opt) {
	if (!run_scripts)
		return 1;

	if (no_scripts_ints && match_patterns(ifd->logical_iface, no_scripts_ints, no_scripts_int))
		return 1;

	char buf[100];

	snprintf(buf, sizeof(buf), "/bin/run-parts %s%s/etc/network/if-%s.d", ignore_failures ? "" : "--exit-on-error ", verbose ? "--verbose " : "", opt);

	int result = (*exec) (buf);

	return ignore_failures ? 1 : result;
}

int iface_preup(interface_defn *iface) {
	set_environ(iface, "start", "pre-up");

	if (!iface->method->up(iface, check))
		return -1;

	if (!execute_options(iface, doit, "pre-up"))
		return 0;

	if (!execute_scripts(iface, doit, "pre-up"))
		return 0;

	return 1;
}

int iface_postup(interface_defn *iface) {
	set_environ(iface, "start", "post-up");

	if (!iface->method->up(iface, doit))
		return 0;

	if (!execute_options(iface, doit, "up"))
		return 0;

	if (!execute_scripts(iface, doit, "up"))
		return 0;

	return 1;
}

int iface_up(interface_defn *iface) {
	int result = iface_preup(iface);

	if (result != 1)
		return result;

	return iface_postup(iface);
}

int iface_predown(interface_defn *iface) {
	if (!no_act) {
		char pidfilename[100];

		make_pidfile_name(pidfilename, sizeof(pidfilename), "ifup", iface);
		FILE *pidfile = fopen(pidfilename, "r");

		if (pidfile) {
			int pid;

			if (fscanf(pidfile, "%d", &pid) == 1) {
				if (verbose)
					fprintf(stderr, "Terminating ifup (pid %d)\n", pid);

				kill((pid_t) - pid, SIGTERM);
			}

			fclose(pidfile);
			unlink(pidfilename);
		}
	}

	set_environ(iface, "stop", "pre-down");

	if (!iface->method->down(iface, check))
		return -1;

	if (!execute_scripts(iface, doit, "down"))
		return 0;

	if (!execute_options(iface, doit, "down"))
		return 0;

	return 1;
}

int iface_postdown(interface_defn *iface) {
	if (!iface->method->down(iface, doit))
		return 0;

	set_environ(iface, "stop", "post-down");

	if (!execute_scripts(iface, doit, "post-down"))
		return 0;

	if (!execute_options(iface, doit, "post-down"))
		return 0;

	return 1;
}

int iface_down(interface_defn *iface) {
	int result = iface_predown(iface);

	if (result != 1)
		return result;

	return iface_postdown(iface);
}

int iface_list(interface_defn *iface) {
	printf("%s\n", iface->real_iface);

	return 0;
}

int iface_query(interface_defn *iface) {
	for (int i = 0; i < iface->n_options; i++)
		printf("%s: %s\n", iface->option[i].name, iface->option[i].value);

	return 0;
}

static void addstr(char **buf, size_t *len, size_t *pos, const char *str, size_t strlen) {
	assert(*len >= *pos);
	assert(*len == 0 || (*buf)[*pos] == '\0');

	if (*pos + strlen >= *len) {
		char *newbuf;

		newbuf = realloc(*buf, *len * 2 + strlen + 1);
		if (!newbuf) {
			perror("realloc");
			exit(1);	/* a little ugly */
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


static char *parse(const char *command, interface_defn *ifd) {
	char *result = NULL;
	size_t pos = 0, len = 0;
	size_t old_pos[MAX_OPT_DEPTH] = { 0 };
	int okay[MAX_OPT_DEPTH] = { 1 };
	int opt_depth = 1;

	while (*command) {
		switch (*command) {
		default:
			addstr(&result, &len, &pos, command, 1);
			command++;
			break;

		case '\\':
			if (command[1]) {
				addstr(&result, &len, &pos, command + 1, 1);
				command += 2;
			} else {
				addstr(&result, &len, &pos, command, 1);
				command++;
			}
			break;

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

		case '%':
			{
				char *nextpercent;
				size_t namelen;
				char pat = 0, rep = 0;
				char *varvalue;

				command++;
				nextpercent = strchr(command, '%');
				namelen = nextpercent - command;

				if (!nextpercent) {
					errno = EUNBALPER;
					free(result);
					return NULL;
				}

				/* %var/p/r% */
				if (*(nextpercent - 4) == '/') {
					pat = *(nextpercent - 3);
					rep = *(nextpercent - 1);
					namelen -= 4;
				}

				varvalue = get_var(command, namelen, ifd);

				if (varvalue) {
					for (char *position = varvalue; *position; position++)
						if (*position == pat)
							*position = rep;

					addstr(&result, &len, &pos, varvalue, strlen(varvalue));
					free(varvalue);
				} else {
					if (opt_depth == 1)
						fprintf(stderr, "Missing required variable: %.*s\n", (int)namelen, command);

					okay[opt_depth - 1] = 0;
				}

				command = nextpercent + 1;

				break;
			}
		}
	}

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

	return result;
}

int execute(const char *command, interface_defn *ifd, execfn *exec) {
	char *out;
	int ret;

	out = parse(command, ifd);
	if (!out)
		return 0;

	ret = (*exec) (out);
	free(out);

	return ret;
}

int strncmpz(const char *l, const char *r, size_t llen) {
	int i = strncmp(l, r, llen);

	if (i == 0)
		return -r[llen];
	else
		return i;
}

char *get_var(const char *id, size_t idlen, interface_defn *ifd) {
	if (strncmpz(id, "iface", idlen) == 0)
		return strdup(ifd->real_iface);

	for (int i = 0; i < ifd->n_options; i++) {
		if (strncmpz(id, ifd->option[i].name, idlen) == 0) {
			if (!ifd->option[i].value)
				return NULL;

			if (strlen(ifd->option[i].value) > 0)
				return strdup(ifd->option[i].value);
			else
				return NULL;
		}
	}

	return NULL;
}

bool var_true(const char *id, interface_defn *ifd) {
	char *varvalue = get_var(id, strlen(id), ifd);

	if (varvalue) {
		if (atoi(varvalue) || strcasecmp(varvalue, "on") == 0 || strcasecmp(varvalue, "true") == 0 || strcasecmp(varvalue, "yes") == 0) {
			free(varvalue);
			return true;
		} else {
			free(varvalue);
			return false;
		}
	} else {
		return false;
	}
}

bool var_set(const char *id, interface_defn *ifd) {
	char *varvalue = get_var(id, strlen(id), ifd);

	if (varvalue) {
		free(varvalue);
		return true;
	} else {
		return false;
	}
}

bool var_set_anywhere(const char *id, interface_defn *ifd) {
	for (interface_defn *currif = defn->ifaces; currif; currif = currif->next) {
		if (strcmp(ifd->logical_iface, currif->logical_iface) == 0) {
			char *varvalue = get_var(id, strlen(id), currif);

			if (varvalue) {
				free(varvalue);
				return true;
			}
		}
	}

	return false;
}

static int popen2(FILE **in, FILE **out, char *command, ...) {
	va_list ap;
	char *argv[11] = { command };
	int argc;
	int infd[2], outfd[2];
	pid_t pid;

	argc = 1;
	va_start(ap, command);

	while ((argc < 10) && (argv[argc] = va_arg(ap, char *)))
		argc++;

	argv[argc] = NULL;	/* make sure */
	va_end(ap);

	if (pipe(infd) != 0)
		return 0;

	if (pipe(outfd) != 0) {
		close(infd[0]);
		close(infd[1]);
		return 0;
	}

	fflush(NULL);

	switch (pid = fork()) {
	case -1:		/* failure */
		close(infd[0]);
		close(infd[1]);
		close(outfd[0]);
		close(outfd[1]);
		return 0;

	case 0:		/* child */
		/* release the current directory */
		chdir("/");
		dup2(infd[0], 0);
		dup2(outfd[1], 1);
		close(infd[0]);
		close(infd[1]);
		close(outfd[0]);
		close(outfd[1]);
		execvp(command, argv);
		exit(127);

	default:		/* parent */
		*in = fdopen(infd[1], "w");
		*out = fdopen(outfd[0], "r");
		close(infd[0]);
		close(outfd[1]);
		return pid;
	}

	/* unreached */
}

bool run_mapping(const char *physical, char *logical, int len, mapping_defn *map) {
	FILE *in, *out;
	int status;
	pid_t pid;
	bool result = false;

	pid = popen2(&in, &out, map->script, physical, NULL);
	if (pid == 0) {
		fprintf(stderr, "Could not execute mapping script %s on %s: %s\n", map->script, physical, strerror(errno));
		return false;
	}

	signal(SIGPIPE, SIG_IGN);

	for (int i = 0; i < map->n_mappings; i++)
		fprintf(in, "%s\n", map->mapping[i]);

	fclose(in);

	signal(SIGPIPE, SIG_DFL);

	waitpid(pid, &status, 0);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		if (fgets(logical, len, out)) {
			char *pch = logical + strlen(logical) - 1;

			while (pch >= logical && isspace(*pch))
				*(pch--) = '\0';

			result = true;
		} else {
			fprintf(stderr, "No output from mapping script %s on %s\n", map->script, physical);
		}
	} else {
		fprintf(stderr, "Error trying to executing mapping script %s on %s\n", map->script, physical);
	}

	fclose(out);

	return result;
}
