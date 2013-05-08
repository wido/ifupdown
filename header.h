#ifndef HEADER_H
#define HEADER_H

#include <stdbool.h>

typedef struct address_family address_family;
typedef struct method method;
typedef struct conversion conversion;
typedef struct option_default option_default;
typedef struct interfaces_file interfaces_file;
typedef struct allowup_defn allowup_defn;
typedef struct interface_defn interface_defn;
typedef struct variable variable;
typedef struct mapping_defn mapping_defn;
typedef int (execfn)(char *command);
typedef int (command_set)(interface_defn * ifd, execfn * e);
struct address_family
{
    char *name;
    int n_methods;
    method *method;
};
struct method
{
    char *name;
    command_set *up, *down;
    conversion *conversions;
    option_default *defaults;
};
struct conversion
{
    char *option;
    char *newoption;
    void (*fn)(interface_defn *, char **, int, char **);
    int argc;
    char **argv;
};

struct option_default
{
    char *option;
    char *value;
};
struct interfaces_file
{
    allowup_defn *allowups;
    interface_defn *ifaces;
    mapping_defn *mappings;
};
struct allowup_defn
{
    allowup_defn *next;

    char *when;
    int max_interfaces;
    int n_interfaces;
    char **interfaces;
};
struct interface_defn
{
    interface_defn *next;

    char *logical_iface;
    char *real_iface;

    address_family *address_family;
    method *method;

    int automatic;

    int max_options;
    int n_options;
    variable *option;
};
struct variable
{
    char *name;
    char *value;
};
struct mapping_defn
{
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
variable * set_variable(char *filename, char *name, char *value, 
                variable **var, int *n_vars, int *max_vars);
void convert_variables(char *filename, conversion *conversions, 
                interface_defn *ifd);
interfaces_file *read_interfaces(char *filename);
interfaces_file *read_interfaces_defn(interfaces_file *defn, char *filename);
allowup_defn *find_allowup(interfaces_file *defn, char *name);
int doit(char *str);
int execute_options(interface_defn * ifd, execfn * exec, char *opt);
int execute_scripts(interface_defn * ifd, execfn * exec, char *opt);
int iface_preup(interface_defn * iface);
int iface_postup(interface_defn * iface);
int iface_up(interface_defn * iface);
int iface_predown(interface_defn * iface);
int iface_postdown(interface_defn * iface);
int iface_down(interface_defn * iface);
int iface_list(interface_defn * iface);
int iface_query(interface_defn * iface);
int execute(char *command, interface_defn * ifd, execfn * exec);
int strncmpz(char *l, char *r, size_t llen);
char *get_var(char *id, size_t idlen, interface_defn * ifd);
int var_true(char *id, interface_defn * ifd);
int var_set(char *id, interface_defn * ifd);
int var_set_anywhere(char *id, interface_defn * ifd);
int run_mapping(char *physical, char *logical, int len, mapping_defn * map);
extern int no_act;
extern int verbose;
extern int run_scripts;
extern bool no_loopback;
extern interfaces_file *defn;
extern address_family addr_link;
extern address_family addr_inet;
extern address_family addr_inet6;
extern address_family addr_ipx;
extern address_family addr_can;
extern address_family addr_meta;

#endif /* HEADER_H */
