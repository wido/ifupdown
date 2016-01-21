#ifndef HEADER_H
#define HEADER_H

#include <stdbool.h>
#include <string.h>

typedef struct address_family address_family;
typedef struct method method;
typedef struct conversion conversion;
typedef struct option_default option_default;
typedef struct interfaces_file interfaces_file;
typedef struct allowup_defn allowup_defn;
typedef struct interface_defn interface_defn;
typedef struct variable variable;
typedef struct mapping_defn mapping_defn;
typedef int (execfn) (const char *command);
typedef int (command_set) (interface_defn *ifd, execfn *e);

struct address_family {
	char *name;
	int n_methods;
	method *method;
};

struct method {
	char *name;
	command_set *up, *down;
	conversion *conversions;
	option_default *defaults;
};

struct conversion {
	char *option;
	char *newoption;
	void (*fn) (interface_defn *, char **, int, char **);
	int argc;
	char **argv;
};

struct option_default {
	char *option;
	char *value;
};

struct interfaces_file {
	allowup_defn *allowups;
	interface_defn *ifaces;
	mapping_defn *mappings;
};

struct allowup_defn {
	allowup_defn *next;

	char *when;
	int max_interfaces;
	int n_interfaces;
	char **interfaces;
};

struct interface_defn {
	interface_defn *next;

	char *logical_iface;
	char *real_iface;

	address_family *address_family;
	method *method;

	int max_options;
	int n_options;
	variable *option;
};

struct variable {
	char *name;
	char *value;
};

struct mapping_defn {
	mapping_defn *next;

	int max_matches;
	int n_matches;
	char **match;

	char *script;

	int max_mappings;
	int n_mappings;
	char **mapping;
};

#define MAX_OPT_DEPTH 10
#define EUNBALBRACK 10001
#define EUNDEFVAR   10002
#define MAX_VARNAME    32
#define EUNBALPER   10000
#ifndef RUN_DIR
#define RUN_DIR "/run/network/"
#endif

#ifndef LO_IFACE
#define LO_IFACE "lo"
#endif

extern address_family *addr_fams[];

variable *set_variable(const char *name, const char *value, variable **var, int *n_vars, int *max_vars);
void convert_variables(conversion *conversions, interface_defn *ifd);
interfaces_file *read_interfaces(const char *filename);
allowup_defn *find_allowup(interfaces_file *defn, const char *name);
bool match_patterns(const char *string, int argc, char *argv[]);
int doit(const char *str);
int iface_preup(interface_defn *iface);
int iface_postup(interface_defn *iface);
int iface_up(interface_defn *iface);
int iface_predown(interface_defn *iface);
int iface_postdown(interface_defn *iface);
int iface_down(interface_defn *iface);
int iface_list(interface_defn *iface);
int iface_query(interface_defn *iface);
int execute(const char *command, interface_defn *ifd, execfn *exec);
int strncmpz(const char *l, const char *r, size_t llen);

#define strlmatch(l,r) strncmp(l,r,strlen(r))

char *get_var(const char *id, size_t idlen, interface_defn *ifd);
bool var_true(const char *id, interface_defn *ifd);
bool var_set(const char *id, interface_defn *ifd);
bool var_set_anywhere(const char *id, interface_defn *ifd);
bool run_mapping(const char *physical, char *logical, int len, mapping_defn *map);
bool make_pidfile_name(char *name, size_t size, const char *command, interface_defn *fd);

extern const char *argv0;
extern bool no_act;
extern bool do_all;
extern bool verbose;
extern bool run_scripts;
extern bool no_loopback;
extern bool ignore_failures;
extern interfaces_file *defn;
extern address_family addr_link;
extern address_family addr_inet;
extern address_family addr_inet6;
extern address_family addr_ipx;
extern address_family addr_can;
extern address_family addr_meta;

extern char **no_auto_down_int;
extern int no_auto_down_ints;
extern char **no_scripts_int;
extern int no_scripts_ints;

#endif				/* HEADER_H */
