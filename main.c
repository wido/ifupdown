#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "header.h"

const char *argv0;
bool no_act = false;
bool run_scripts = true;
bool verbose = false;
bool no_loopback = false;
bool ignore_failures = false;

interfaces_file *defn;

static char lockfile[] = RUN_DIR ".ifstate.lock";
static char statefile[] = RUN_DIR "ifstate";
static char tmpstatefile[] = RUN_DIR ".ifstate.tmp";

static void usage() {
	fprintf(stderr, "%s: Use --help for help\n", argv0);
	exit(1);
}

static void version() {
	printf("%s version " IFUPDOWN_VERSION "\n"
		"\n"
		"Copyright (c) 1999-2009 Anthony Towns\n"
		"              2010-2015 Andrew Shadura\n"
		"              2015      Guus Sliepen\n"
		"\n"
		"This program is free software; you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation; either version 2 of the License, or (at\n"
		"your option) any later version.\n", argv0);

	exit(0);
}

static void help(int (*cmds) (interface_defn *)) {
	printf("Usage: %s <options> <ifaces...>\n", argv0);

	if ((cmds == iface_list) || (cmds == iface_query)) {
		printf("       %s <options> --list\n", argv0);
		printf("       %s --state <ifaces...>\n", argv0);
	}

	printf("\n"
		"Options:\n"
		"\t-h, --help             this help\n"
		"\t-V, --version          copyright and version information\n"
		"\t-a, --all              process all interfaces marked \"auto\"\n"
		"\t--allow CLASS          ignore non-\"allow-CLASS\" interfaces\n"
		"\t-i, --interfaces FILE  use FILE for interface definitions\n"
		"\t-X, --exclude PATTERN  exclude interfaces from the list of\n"
		"\t                       interfaces to operate on by a PATTERN\n");

	if (!(cmds == iface_list) && !(cmds == iface_query))
		printf(	"\t-n, --no-act           print out what would happen, but don't do it\n"
			"\t                       (note that this option doesn't disable mappings)\n");

	printf(	"\t-v, --verbose          print out what would happen before doing it\n"
		"\t-o OPTION=VALUE        set OPTION to VALUE as though it were in\n"
		"\t                       /etc/network/interfaces\n"
		"\t--no-mappings          don't run any mappings\n"
		"\t--no-scripts           don't run any hook scripts\n"
		"\t--no-loopback          don't act specially on the loopback device\n");

	if (!(cmds == iface_list) && !(cmds == iface_query))
		printf(	"\t--force                force de/configuration\n"
			"\t--ignore-errors        ignore errors\n");

	if ((cmds == iface_list) || (cmds == iface_query))
		printf(	"\t--list                 list all matching known interfaces\n"
			"\t--state                show the state of specified interfaces\n");

	exit(0);
}

static int lock_fd(int fd) {
	struct flock lock = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};

	if (fcntl(fd, F_SETLKW, &lock) < 0)
		return -1;

	return 0;
}

static FILE *lock_state() {
	FILE *lock_fp = fopen(lockfile, no_act ? "r" : "a+");

	if (lock_fp == NULL) {
		if (!no_act) {
			fprintf(stderr, "%s: failed to open lockfile %s: %s\n", argv0, lockfile, strerror(errno));
			exit(1);
		} else {
			return NULL;
		}
	}

	int flags = fcntl(fileno(lock_fp), F_GETFD);

	if (flags < 0 || fcntl(fileno(lock_fp), F_SETFD, flags | FD_CLOEXEC) < 0) {
		fprintf(stderr, "%s: failed to set FD_CLOEXEC on lockfile %s: %s\n", argv0, lockfile, strerror(errno));
		exit(1);
	}

	if (lock_fd(fileno(lock_fp)) < 0) {
		if (!no_act) {
			fprintf(stderr, "%s: failed to lock lockfile %s: %s\n", argv0, lockfile, strerror(errno));
			exit(1);
		}
	}

	return lock_fp;
}

static char *strip(char *buf) {
	char *pch = buf + strlen(buf) - 1;
	while (pch > buf && isspace(*pch))
		*pch-- = '\0';

	while (isspace(*buf))
		buf++;

	return buf;
}

static FILE *lock_interface(const char *iface, char **state) {
	char filename[sizeof statefile + strlen(iface) + 2];
	snprintf(filename, sizeof filename, "%s.%s", statefile, iface);

	FILE *lock_fp = fopen(filename, no_act ? "r" : "a+");

	if (lock_fp == NULL) {
		if (!no_act) {
			fprintf(stderr, "%s: failed to open lockfile %s: %s\n", argv0, filename, strerror(errno));
			exit(1);
		} else {
			return NULL;
		}
	}

	int flags = fcntl(fileno(lock_fp), F_GETFD);

	if (flags < 0 || fcntl(fileno(lock_fp), F_SETFD, flags | FD_CLOEXEC) < 0) {
		fprintf(stderr, "%s: failed to set FD_CLOEXEC on lockfile %s: %s\n", argv0, filename, strerror(errno));
		exit(1);
	}

	struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET};

	if (fcntl(fileno(lock_fp), F_SETLK, &lock) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			fprintf(stderr, "%s: waiting for lock on %s\n", argv0, filename);
			if (fcntl(fileno(lock_fp), F_SETLKW, &lock) < 0) {
				if (!no_act) {
					fprintf(stderr, "%s: failed to lock lockfile %s: %s\n", argv0, filename, strerror(errno));
					exit(1);
				}
			}
		} else if (!no_act) {
			fprintf(stderr, "%s: failed to lock lockfile %s: %s\n", argv0, filename, strerror(errno));
			exit(1);
		}
	}

	if (state) {
		char buf[80];
		char *p = fgets(buf, sizeof buf, lock_fp);
		if(p) {
			p = strip(buf);
			*state = *p ? strdup(p) : NULL;
		} else {
			*state = NULL;
		}
	}

	return lock_fp;
}

static void read_all_state(char ***ifaces, int *n_ifaces) {
	FILE *lock_fp = lock_state(argv0);
	FILE *state_fp = fopen(statefile, no_act ? "r" : "a+");

	if (state_fp == NULL) {
		if (!no_act) {
			fprintf(stderr, "%s: failed to open statefile %s: %s\n", argv0, statefile, strerror(errno));
			exit(1);
		} else {
			goto end;
		}
	}

	if (!no_act) {
		int flags = fcntl(fileno(state_fp), F_GETFD);

		if (flags < 0 || fcntl(fileno(state_fp), F_SETFD, flags | FD_CLOEXEC) < 0) {
			fprintf(stderr, "%s: failed to set FD_CLOEXEC on statefile %s: %s\n", argv0, statefile, strerror(errno));
			exit(1);
		}
	}

	*n_ifaces = 0;
	*ifaces = NULL;

	char buf[80];
	char *p;

	while ((p = fgets(buf, sizeof buf, state_fp)) != NULL) {
		(*n_ifaces)++;
		*ifaces = realloc(*ifaces, sizeof(**ifaces) * *n_ifaces);
		(*ifaces)[(*n_ifaces) - 1] = strdup(strip(buf));
	}

	for (int i = 0; i < ((*n_ifaces) / 2); i++) {
		char *temp = (*ifaces)[i];

		(*ifaces)[i] = (*ifaces)[(*n_ifaces) - i - 1];
		(*ifaces)[(*n_ifaces) - i - 1] = temp;
	}

 end:
	if (state_fp)
		fclose(state_fp);

	if (lock_fp)
		fclose(lock_fp);
}

static void update_state(const char *iface, const char *state, FILE *lock_fp) {
	if (lock_fp) {
		rewind(lock_fp);
		ftruncate(fileno(lock_fp), 0);
		fprintf(lock_fp, "%s\n", state ? state : "");
		fflush(lock_fp);
	}

	lock_fp = lock_state(argv0);
	FILE *state_fp = fopen(statefile, no_act ? "r" : "a+");

	if (state_fp == NULL) {
		if (!no_act) {
			fprintf(stderr, "%s: failed to open statefile %s: %s\n", argv0, statefile, strerror(errno));
			exit(1);
		} else {
			goto end;
		}
	}

	if (no_act)
		goto end;

	int flags = fcntl(fileno(state_fp), F_GETFD);

	if (flags < 0 || fcntl(fileno(state_fp), F_SETFD, flags | FD_CLOEXEC) < 0) {
		fprintf(stderr, "%s: failed to set FD_CLOEXEC on statefile %s: %s\n", argv0, statefile, strerror(errno));
		exit(1);
	}

	if (lock_fd(fileno(state_fp)) < 0) {
		fprintf(stderr, "%s: failed to lock statefile %s: %s\n", argv0, statefile, strerror(errno));
		exit(1);
	}

	FILE *tmp_fp = fopen(tmpstatefile, "w");

	if (tmp_fp == NULL) {
		fprintf(stderr, "%s: failed to open temporary statefile %s: %s\n", argv0, tmpstatefile, strerror(errno));
		exit(1);
	}

	char buf[80];
	char *p;

	while ((p = fgets(buf, sizeof buf, state_fp)) != NULL) {
		char *pch = strip(buf);

		if (strncmp(iface, pch, strlen(iface)) == 0) {
			if (pch[strlen(iface)] == '=') {
				if (state != NULL) {
					fprintf(tmp_fp, "%s=%s\n", iface, state);
					state = NULL;
				}

				continue;
			}
		}

		fprintf(tmp_fp, "%s\n", pch);
	}

	if (state != NULL)
		fprintf(tmp_fp, "%s=%s\n", iface, state);

	fclose(tmp_fp);

	if (rename(tmpstatefile, statefile)) {
		fprintf(stderr, "%s: failed to overwrite statefile %s: %s\n", argv0, statefile, strerror(errno));
		exit(1);
	}

 end:
	if (state_fp)
		fclose(state_fp);

	if (lock_fp)
		fclose(lock_fp);
}

static void sanitize_file_name(char *name) {
	for (; *name; name++)
		if (*name == '/')
			*name = '.';
}

bool make_pidfile_name(char *name, size_t size, const char *command, interface_defn * ifd) {
	char *iface = strdup(ifd->real_iface);

	if (!iface)
		return false;

	sanitize_file_name(iface);

	int n = snprintf(name, size, RUN_DIR "%s-%s.pid", command, iface);

	free(iface);

	if (n < 0 || (size_t) n >= size)
		return false;

	return true;
}

/* Ensure stdin, stdout and stderr are valid, open filedescriptors */
static void check_stdio(void) {
	for (int i = 0; i <= 2; i++) {
		errno = 0;
		if (fcntl(i, F_GETFD) == -1) {
			if (errno == EBADF) {
				/* filedescriptor closed, try to open /dev/null in its place */
				if (open("/dev/null", 0) != i) {
					fprintf(stderr, "%s: fd %d not available; aborting\n", argv0, i);
					exit(2);
				}
			} else {
				/* some other problem -- eeek */
				perror(argv0);
				exit(2);
			}
		}
	}
}

typedef int (*cmds_t)(interface_defn *);

/* Determine whether we are being called as ifup, ifdown or ifquery */
static cmds_t determine_command(void) {
	const char *command;

	if ((command = strrchr(argv0, '/')))
		command++;	/* first char after / */
	else
		command = argv0;	/* no /'s in argv0 */

	if (strcmp(command, "ifup") == 0) {
		return iface_up;
	} else if (strcmp(command, "ifdown") == 0) {
		ignore_failures = true;
		return iface_down;
	} else if (strcmp(command, "ifquery") == 0) {
		no_act = true;
		return iface_query;
	} else {
		fprintf(stderr, "This command should be called as ifup, ifdown, or ifquery\n");
		exit(1);
	}
}

static cmds_t cmds = NULL;
static bool do_all = false;
static bool run_mappings = true;
static bool force = false;
static bool list = false;
static bool state_query = false;
static char *allow_class = NULL;
static char *interfaces = "/etc/network/interfaces";
static char **excludeint = NULL;
static int excludeints = 0;
static variable *option = NULL;
static int n_options = 0;
static int max_options = 0;
static int n_target_ifaces;
static char **target_iface;

static void parse_options(int *argc, char **argv[]) {
	static const struct option long_opts[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{"verbose", no_argument, NULL, 'v'},
		{"all", no_argument, NULL, 'a'},
		{"allow", required_argument, NULL, 3},
		{"interfaces", required_argument, NULL, 'i'},
		{"exclude", required_argument, NULL, 'X'},
		{"no-act", no_argument, NULL, 'n'},
		{"no-mappings", no_argument, NULL, 1},
		{"no-scripts", no_argument, NULL, 4},
		{"no-loopback", no_argument, NULL, 5},
		{"force", no_argument, NULL, 2},
		{"ignore-errors", no_argument, NULL, 7},
		{"option", required_argument, NULL, 'o'},
		{"list", no_argument, NULL, 'l'},
		{"state", no_argument, NULL, 6},
		{0, 0, 0, 0}
	};

	for (;;) {
		int c = getopt_long(*argc, *argv, "X:s:i:o:hVvnal", long_opts, NULL);

		if (c == EOF)
			break;

		switch (c) {
		case 'i':
			interfaces = strdup(optarg);
			break;

		case 'v':
			verbose = true;
			break;

		case 'a':
			do_all = true;
			break;

		case 3:
			allow_class = strdup(optarg);
			break;

		case 'n':
			if ((cmds == iface_list) || (cmds == iface_query))
				usage();
			no_act = true;
			break;

		case 1:
			run_mappings = false;
			break;

		case 4:
			run_scripts = false;
			break;

		case 5:
			no_loopback = true;
			break;

		case 2:
			if ((cmds == iface_list) || (cmds == iface_query))
				usage();
			force = true;
			break;

		case 7:
			ignore_failures = true;
			break;

		case 'X':
			excludeints++;
			excludeint = realloc(excludeint, excludeints * sizeof *excludeint);
			if (excludeint == NULL) {
				perror(argv0);
				exit(1);
			}
			excludeint[excludeints - 1] = strdup(optarg);
			break;

		case 'o':
			{
				char *name = strdup(optarg);
				char *val = strchr(name, '=');

				if (val == NULL) {
					fprintf(stderr, "Error in --option \"%s\" -- no \"=\" character\n", optarg);
					exit(1);
				}
				*val++ = '\0';

				if (strcmp(name, "post-up") == 0)
					strcpy(name, "up");

				if (strcmp(name, "pre-down") == 0)
					strcpy(name, "down");

				set_variable(name, val, &option, &n_options, &max_options);
				free(name);

				break;
			}

		case 'l':
			if (!(cmds == iface_query))
				usage();

			list = true;
			cmds = iface_list;
			break;

		case 'h':
			help(cmds);
			break;

		case 'V':
			version();
			break;

		case 6: /* --state */
			if (cmds != iface_query)
				usage();

			state_query = true;
			break;

		default:
			usage();
			break;
		}
	}

	*argc -= optind;
	*argv += optind;
}

/* Report the state of interfaces. Return 0 (success) if all reported interfaces are up, 1 (failure) otherwise */
static int do_state(int n_target_ifaces, char *target_iface[]) {
	char **up_ifaces;
	int n_up_ifaces;

	read_all_state(&up_ifaces, &n_up_ifaces);
	int ret = 0;

	if (n_target_ifaces == 0) {
		for (int i = 0; i < n_up_ifaces; i++)
			puts(up_ifaces[i]);
	} else {
		for (int j = 0; j < n_target_ifaces; j++) {
			size_t l = strlen(target_iface[j]);
			bool found = false;

			for (int i = 0; i < n_up_ifaces; i++) {
				if (strncmp(target_iface[j], up_ifaces[i], l) == 0) {
					if (up_ifaces[i][l] == '=') {
						puts(up_ifaces[i]);
						found = true;
						break;
					}
				}
			}

			if (!found)
				ret = 1;
		}
	}

	return ret;
}

/* Check non-option arguments and build a list of interfaces to act upon */
static void select_interfaces(int argc, char *argv[]) {
	if (argc > 0 && (do_all || list)) {
		fprintf(stderr, "%s: either use the --all/--list options, or specify interface(s), but not both\n", argv0);
		usage();
	}

	if (argc == 0 && !do_all && !list) {
		fprintf(stderr, "%s: no interface(s) specified\n", argv0);
		usage();
	}

	if (do_all && (cmds == iface_query))
		usage();

	defn = read_interfaces(interfaces);

	if (!defn) {
		fprintf(stderr, "%s: couldn't read interfaces file \"%s\"\n", argv0, interfaces);
		exit(1);
	}

	if (do_all || list) {
		if ((cmds == iface_list) || (cmds == iface_up)) {
			allowup_defn *autos = find_allowup(defn, allow_class ? allow_class : "auto");

			target_iface = autos ? autos->interfaces : NULL;
			n_target_ifaces = autos ? autos->n_interfaces : 0;
		} else if (cmds == iface_down) {
			read_all_state(&target_iface, &n_target_ifaces);
		} else {
			fprintf(stderr, "%s: can't tell if interfaces are going up or down\n", argv0);
			usage();
		}
	} else {
		target_iface = argv;
		n_target_ifaces = argc;
	}
}

static interface_defn meta_iface = {
	.next = NULL,
	.real_iface = "--all",
	.address_family = &addr_meta,
	.automatic = true,
	.max_options = 0,
	.n_options = 0,
	.option = NULL
};

/* Run pre hooks for the meta interface when calling ifup/down with --all */
static void do_pre_all(void) {
	meta_iface.logical_iface = allow_class ? allow_class : "auto";
	meta_iface.method = meta_iface.address_family->method;

	bool okay = true;

	if (cmds == iface_up)
		okay = iface_preup(&meta_iface);

	if (cmds == iface_down)
		okay = iface_predown(&meta_iface);

	if (!okay) {
		fprintf(stderr, "%s: pre-%s script failed.\n", argv0, cmds == iface_up ? "up" : "down");
		exit(1);
	}
}

/* Run post hooks for the meta interface when calling ifup/down with --all */
static void do_post_all(void) {
	bool okay = true;

	if (cmds == iface_up)
		okay = iface_postup(&meta_iface);

	if (cmds == iface_down)
		okay = iface_postdown(&meta_iface);

	if (!okay) {
		fprintf(stderr, "%s: post-%s script failed.\n", argv0, cmds == iface_up ? "up" : "down");
		exit(1);
	}
}

static bool match_patterns(const char *string, int argc, char *argv[]) {
	if (!argc || !argv || !string)
		return false;

	for (int i = 0; i < argc; i++)
		if (fnmatch(argv[i], string, 0) == 0)
			return true;

	return false;
}

/* Check whether we should ignore the given interface */
static bool ignore_interface(const char *iface) {
	/* If --allow is used, ignore interfaces that are not in the given class */
	if (allow_class != NULL) {
		allowup_defn *allowup = find_allowup(defn, allow_class);

		if (allowup == NULL) // empty class
			return true;

		bool found = false;

		for (int i = 0; i < allowup->n_interfaces; i++) {
			if (strcmp(allowup->interfaces[i], iface) == 0) {
				found = true;
				break;
			}
		}

		if (!found)
			return true;
	}

	/* Ignore interfaces specified with --exclude */
	if ((excludeints != 0 && match_patterns(iface, excludeints, excludeint)))
		return true;

	return false;
}


static bool do_interface(const char *target_iface) {
	/* Split into physical and logical interface */

	char iface[80], liface[80];

	strncpy(iface, target_iface, sizeof(iface));
	iface[sizeof(iface) - 1] = '\0';

	char *pch;

	if ((pch = strchr(iface, '='))) {
		*pch = '\0';
		strncpy(liface, pch + 1, sizeof(liface));
		liface[sizeof(liface) - 1] = '\0';
	} else {
		strncpy(liface, iface, sizeof(liface));
		liface[sizeof(liface) - 1] = '\0';
	}

	/* Check if we really want to process this interface */

	if(ignore_interface(iface))
		return true;

	/* Bail out if we are being called recursively on the same interface */

	char envname[160];
	snprintf(envname, sizeof envname, "IFUPDOWN_%s", iface);
	char *envval = getenv(envname);
	if(envval) {
		fprintf(stderr, "%s: recursion detected for interface %s in %s phase\n", argv0, iface, envval);
		return false;
	}

	/* Are we configuring a VLAN interface? If so, lock the parent interface as well. */

	char piface[80];
	FILE *plock = NULL;
	strncpy(piface, iface, sizeof piface);
	if ((pch = strchr(piface, '.'))) {
		*pch = '\0';
		snprintf(envname, sizeof envname, "IFUPDOWN_%s", piface);
		char *envval = getenv(envname);
		if(envval) {
			fprintf(stderr, "%s: recursion detected for parent interface %s in %s phase\n", argv0, piface, envval);
			return false;
		}

		plock = lock_interface(piface, NULL);
	}

	/* Start by locking this interface */

	bool success = false;
	FILE *lock = NULL;
	char *current_state;

	lock = lock_interface(iface, &current_state);

	/* If we are not forcing the command, then exit with success if it is a no-op */

	if (!force) {
		if (cmds == iface_up) {
			if (current_state != NULL) {
				if (!do_all)
					fprintf(stderr, "%s: interface %s already configured\n", argv0, iface);

				success = true;
				goto end;
			}
		} else if (cmds == iface_down) {
			if (current_state == NULL) {
				if (!do_all)
					fprintf(stderr, "%s: interface %s not configured\n", argv0, iface);

				success = true;
				goto end;
			}

			strncpy(liface, current_state, 80);
			liface[79] = 0;
		} else if (cmds == iface_query) {
			if (current_state != NULL) {
				strncpy(liface, current_state, 80);
				liface[79] = 0;
				run_mappings = false;
			}
		} else {
			assert(cmds == iface_list ||cmds == iface_query);
		}
	}

	/* Run mapping scripts if necessary */

	bool have_mapping = false;

	if (((cmds == iface_up) && run_mappings) || (cmds == iface_query)) {
		for (mapping_defn *currmap = defn->mappings; currmap; currmap = currmap->next) {
			for (int i = 0; i < currmap->n_matches; i++) {
				if (fnmatch(currmap->match[i], liface, 0) != 0)
					continue;

				if ((cmds == iface_query) && !run_mappings) {
					if (verbose)
						fprintf(stderr, "Not running mapping scripts for %s\n", liface);

					have_mapping = true;
					break;
				}

				if (verbose)
					fprintf(stderr, "Running mapping script %s on %s\n", currmap->script, liface);

				if(!run_mapping(iface, liface, sizeof(liface), currmap))
					goto end;

				break;
			}
		}
	}

	bool okay = false;
	bool failed = false;

	/* Update the state file already? */

	if (cmds == iface_up) {
		update_state(iface, liface, lock);
	} else if (cmds == iface_down) {
		update_state(iface, NULL, lock);
	} else  {
		assert(cmds == iface_list ||cmds == iface_query);
	}

	/* Handle ifquery --list */

	if (cmds == iface_list) {
		for (interface_defn *currif = defn->ifaces; currif; currif = currif->next)
			if (strcmp(liface, currif->logical_iface) == 0)
				okay = true;

		if (!okay) {
			mapping_defn *currmap;

			for (currmap = defn->mappings; currmap; currmap = currmap->next) {
				for (int i = 0; i < currmap->n_matches; i++) {
					if (fnmatch(currmap->match[i], liface, 0) != 0)
						continue;

					okay = true;
					break;
				}
			}
		}

		if (okay) {
			interface_defn *currif = defn->ifaces;
			currif->real_iface = iface;
			cmds(currif);
			currif->real_iface = NULL;
			success = true;
		}

		goto end;
	}

	/* Run the desired command for all matching logical interfaces */

	for (interface_defn *currif = defn->ifaces; currif; currif = currif->next) {
		if (strcmp(liface, currif->logical_iface) == 0) {
			/* Bring the link up if necessary, but only once for each physical interface */
			if (!okay && (cmds == iface_up)) {
				interface_defn link = {
					.real_iface = iface,
					.logical_iface = liface,
					.max_options = 0,
					.address_family = &addr_link,
					.method = &(addr_link.method[0]),
					.n_options = 0,
					.option = NULL
				};

				convert_variables(link.method->conversions, &link);

				if (!link.method->up(&link, doit))
					break;

				if (link.option)
					free(link.option);
			}

			okay = true;

			for (option_default *o = currif->method->defaults; o && o->option && o->value; o++) {
				bool found = false;

				for (int j = 0; j < currif->n_options; j++) {
					if (strcmp(currif->option[j].name, o->option) == 0) {
						found = true;
						break;
					}
				}

				if (!found)
					set_variable(o->option, o->value, &currif->option, &currif->n_options, &currif->max_options);
			}

			for (int i = 0; i < n_options; i++) {
				if (option[i].value[0] == '\0') {
					if (strcmp(option[i].name, "pre-up") != 0 && strcmp(option[i].name, "up") != 0 && strcmp(option[i].name, "down") != 0 && strcmp(option[i].name, "post-down") != 0) {
						int j;

						for (j = 0; j < currif->n_options; j++) {
							if (strcmp(currif->option[j].name, option[i].name) == 0) {
								currif->n_options--;
								break;
							}
						}

						for (; j < currif->n_options; j++) {
							option[j].name = option[j + 1].name;
							option[j].value = option[j + 1].value;
						}
					} else {
						/* do nothing */
					}
				} else {
					set_variable(option[i].name, option[i].value, &currif->option, &currif->n_options, &currif->max_options);
				}
			}

			currif->real_iface = iface;

			convert_variables(currif->method->conversions, currif);

			if (verbose)
				fprintf(stderr, "%s interface %s=%s (%s)\n", (cmds == iface_query) ? "Querying" : "Configuring", iface, liface, currif->address_family->name);

			char pidfilename[100];
			const char *command;

			if ((command = strrchr(argv0, '/')))
				command++;	/* first char after / */
			else
				command = argv0;	/* no /'s in argv0 */

			make_pidfile_name(pidfilename, sizeof(pidfilename), command, currif);

			if (!no_act) {
				FILE *pidfile = fopen(pidfilename, "w");

				if (pidfile) {
					fprintf(pidfile, "%d", getpid());
					fclose(pidfile);
				} else {
					fprintf(stderr, "%s: failed to open pid file %s: %s\n", command, pidfilename, strerror(errno));
				}
			}

			switch (cmds(currif)) {
			case -1:
				fprintf(stderr, "Missing required configuration variables for interface %s/%s.\n", liface, currif->address_family->name);
				failed = true;
				break;

			case 0:
				failed = true;
				break;
				/* not entirely successful */

			case 1:
				failed = false;
				break;
				/* successful */

			default:
				fprintf(stderr, "Unexpected value when configuring interface %s/%s; considering it failed.\n", liface, currif->address_family->name);
				failed = true;
				/* what happened here? */
			}

			if (!no_act)
				unlink(pidfilename);

			currif->real_iface = NULL;

			if (failed)
				break;

			/* Otherwise keep going: this interface may have match with other address families */
		}
	}

	/* Bring the link down if necessary */

	if (okay && (cmds == iface_down)) {
		interface_defn link = {
			.real_iface = iface,
			.logical_iface = liface,
			.max_options = 0,
			.address_family = &addr_link,
			.method = &(addr_link.method[0]),
			.n_options = 0,
			.option = NULL
		};
		convert_variables(link.method->conversions, &link);

		if (!link.method->down(&link, doit))
			goto end;
		if (link.option)
			free(link.option);
	}

	if (!okay && (cmds == iface_query)) {
		if (!run_mappings)
			if (have_mapping)
				okay = true;

		if (!okay) {
			fprintf(stderr, "Unknown interface %s\n", iface);
			goto end;
		}
	}

	/* Update the state */

	if (!okay && !force) {
		fprintf(stderr, "Ignoring unknown interface %s=%s.\n", iface, liface);
		update_state(iface, NULL, lock);
	} else {
		if (cmds == iface_up) {
			if ((current_state == NULL) || (no_act)) {
				if (failed == true) {
					printf("Failed to bring up %s.\n", liface);
					update_state(iface, NULL, lock);
					goto end;
				} else {
					update_state(iface, liface, lock);
				}
			} else {
				update_state(iface, liface, lock);
			}
		} else if (cmds == iface_down) {
			update_state(iface, NULL, lock);
		} else {
			assert(cmds == iface_list ||cmds == iface_query);
		}
	}

	success = true;

end:
	if(lock)
		fclose(lock);

	if(plock)
		fclose(plock);

	return success;
}

int main(int argc, char *argv[]) {
	argv0 = argv[0];

	check_stdio();

	cmds = determine_command();

	parse_options(&argc, &argv);

	mkdir(RUN_DIR, 0755);

	if (state_query)
		return do_state(argc, argv);

	select_interfaces(argc, argv);

	if (do_all)
		do_pre_all();

	bool success = true;

	for (int i = 0; i < n_target_ifaces; i++)
		success &= do_interface(target_iface[i]);

	if (do_all)
		do_post_all();

	return success ? 0 : 1;
}
