#line 1036 "ifupdown.nw"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#line 1047 "ifupdown.nw"
#include "header.h"
#line 1250 "ifupdown.nw"
#include <errno.h>
#line 1384 "ifupdown.nw"
#include <ctype.h>
#line 1227 "ifupdown.nw"
static int get_line(char **result, size_t *result_len, FILE *f, int *line);
#line 1454 "ifupdown.nw"
static char *next_word(char *buf, char *word, int maxlen);
#line 1688 "ifupdown.nw"
static address_family *get_address_family(address_family *af[], char *name);
#line 1723 "ifupdown.nw"
static method *get_method(address_family *af, char *name);
#line 1784 "ifupdown.nw"
static int duplicate_if(interface_defn *ifa, interface_defn *ifb);
#line 1160 "ifupdown.nw"
interfaces_file *read_interfaces(char *filename) {
	
#line 1197 "ifupdown.nw"
FILE *f;
int line;
#line 1234 "ifupdown.nw"
char *buf = NULL;
size_t buf_len = 0;
#line 1434 "ifupdown.nw"
interface_defn *currif = NULL;
mapping_defn *currmap = NULL;
enum { NONE, IFACE, MAPPING } currently_processing = NONE;
#line 1445 "ifupdown.nw"
char firstword[80];
char *rest;
#line 1162 "ifupdown.nw"
	interfaces_file *defn;

	
#line 1181 "ifupdown.nw"
defn = malloc(sizeof(interfaces_file));
if (defn == NULL) {
	return NULL;
}
defn->max_autointerfaces = defn->n_autointerfaces = 0;
defn->autointerfaces = NULL;
defn->mappings = NULL;
defn->ifaces = NULL;
#line 1165 "ifupdown.nw"
	
#line 1202 "ifupdown.nw"
f = fopen(filename, "r");
if ( f == NULL ) return NULL;
line = 0;

#line 1167 "ifupdown.nw"
	while (
#line 1242 "ifupdown.nw"
get_line(&buf,&buf_len,f,&line)
#line 1167 "ifupdown.nw"
                                             ) {
		
#line 1481 "ifupdown.nw"
rest = next_word(buf, firstword, 80);
if (rest == NULL) continue; /* blank line */

if (strcmp(firstword, "mapping") == 0) {
	
#line 1530 "ifupdown.nw"
currmap = malloc(sizeof(mapping_defn));
if (currmap == NULL) {
	
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1533 "ifupdown.nw"
}
#line 1537 "ifupdown.nw"
currmap->max_matches = 0;
currmap->n_matches = 0;
currmap->match = NULL;

while((rest = next_word(rest, firstword, 80))) {
	if (currmap->max_matches == currmap->n_matches) {
		char **tmp;
		currmap->max_matches = currmap->max_matches * 2 + 1;
		tmp = realloc(currmap->match, 
			sizeof(*tmp) * currmap->max_matches);
		if (tmp == NULL) {
			currmap->max_matches = (currmap->max_matches - 1) / 2;
			
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1550 "ifupdown.nw"
		}
		currmap->match = tmp;
	}

	currmap->match[currmap->n_matches++] = strdup(firstword);
}
#line 1559 "ifupdown.nw"
currmap->script = NULL;

currmap->max_mappings = 0;
currmap->n_mappings = 0;
currmap->mapping = NULL;
#line 1567 "ifupdown.nw"
{
	mapping_defn **where = &defn->mappings;
	while(*where != NULL) {
		where = &(*where)->next;
	}
	*where = currmap;
	currmap->next = NULL;
}
#line 1486 "ifupdown.nw"
	currently_processing = MAPPING;
} else if (strcmp(firstword, "iface") == 0) {
	
#line 1619 "ifupdown.nw"
{
	
#line 1652 "ifupdown.nw"
char iface_name[80];
char address_family_name[80];
char method_name[80];

#line 1622 "ifupdown.nw"
	
#line 1640 "ifupdown.nw"
currif = malloc(sizeof(interface_defn));
if (!currif) {
	
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1643 "ifupdown.nw"
}

#line 1624 "ifupdown.nw"
	
#line 1658 "ifupdown.nw"
rest = next_word(rest, iface_name, 80);
rest = next_word(rest, address_family_name, 80);
rest = next_word(rest, method_name, 80);

if (rest == NULL) {
	
#line 1918 "ifupdown.nw"
fprintf(stderr, "%s:%d: too few parameters for iface line\n", filename, line);
return NULL;
#line 1664 "ifupdown.nw"
}

if (rest[0] != '\0') {
	
#line 1923 "ifupdown.nw"
fprintf(stderr, "%s:%d: too many parameters for iface line\n", filename, line);
return NULL;
#line 1668 "ifupdown.nw"
}

#line 1626 "ifupdown.nw"
	
#line 1674 "ifupdown.nw"
currif->iface = strdup(iface_name);
if (!currif->iface) {
	
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1677 "ifupdown.nw"
}
#line 1627 "ifupdown.nw"
	
#line 1692 "ifupdown.nw"
currif->address_family = get_address_family(addr_fams, address_family_name);
if (!currif->address_family) {
	
#line 1928 "ifupdown.nw"
fprintf(stderr, "%s:%d: unknown address type\n", filename, line);
return NULL;
#line 1695 "ifupdown.nw"
}
#line 1628 "ifupdown.nw"
	
#line 1727 "ifupdown.nw"
currif->method = get_method(currif->address_family, method_name);
if (!currif->method) {
	
#line 1933 "ifupdown.nw"
fprintf(stderr, "%s:%d: unknown method\n", filename, line);
return NULL;
#line 1730 "ifupdown.nw"
	return NULL; /* FIXME */
}
#line 1629 "ifupdown.nw"
	
#line 1750 "ifupdown.nw"
currif->automatic = 1;
currif->max_options = 0;
currif->n_options = 0;
currif->option = NULL;

#line 1631 "ifupdown.nw"
	
#line 1766 "ifupdown.nw"
{
	interface_defn **where = &defn->ifaces; 
	while(*where != NULL) {
		if (duplicate_if(*where, currif)) {
			
#line 1938 "ifupdown.nw"
fprintf(stderr, "%s:%d: duplicate interface\n", filename, line);
return NULL;
#line 1771 "ifupdown.nw"
		}
		where = &(*where)->next;
	}

	*where = currif;
	currif->next = NULL;
}
#line 1632 "ifupdown.nw"
}
#line 1489 "ifupdown.nw"
	currently_processing = IFACE;
} else if (strcmp(firstword, "auto") == 0) {
	
#line 1870 "ifupdown.nw"
while((rest = next_word(rest, firstword, 80))) {
	
#line 1877 "ifupdown.nw"
{
	int i;

	for (i = 0; i < defn->n_autointerfaces; i++) {
		if (strcmp(firstword, defn->autointerfaces[i]) == 0) {
			
#line 1943 "ifupdown.nw"
fprintf(stderr, "%s:%d: interface declared auto twice\n", filename, line);
return NULL;
#line 1883 "ifupdown.nw"
		}
	}
}
#line 1872 "ifupdown.nw"
	
#line 1889 "ifupdown.nw"
if (defn->n_autointerfaces == defn->max_autointerfaces) {
	char **tmp;
	defn->max_autointerfaces *= 2;
	defn->max_autointerfaces++;
	tmp = realloc(defn->autointerfaces, 
		sizeof(*tmp) * defn->max_autointerfaces);
	if (tmp == NULL) {
		
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1897 "ifupdown.nw"
	}
	defn->autointerfaces = tmp;
}

defn->autointerfaces[defn->n_autointerfaces] = strdup(firstword);
defn->n_autointerfaces++;
#line 1873 "ifupdown.nw"
}
#line 1492 "ifupdown.nw"
	currently_processing = NONE;
} else {
	
#line 1499 "ifupdown.nw"
switch(currently_processing) {
	case IFACE:
		
#line 1804 "ifupdown.nw"
{
	int i;

	if (strlen (rest) == 0) {
		
#line 1963 "ifupdown.nw"
fprintf(stderr, "%s:%d: option with empty value\n", filename, line);
return NULL;
#line 1809 "ifupdown.nw"
	}

	if (strcmp(firstword, "up") != 0
	    && strcmp(firstword, "down") != 0
	    && strcmp(firstword, "pre-up") != 0
	    && strcmp(firstword, "post-down") != 0)
        {
		for (i = 0; i < currif->n_options; i++) {
			if (strcmp(currif->option[i].name, firstword) == 0) {
				
#line 1948 "ifupdown.nw"
fprintf(stderr, "%s:%d: duplicate option\n", filename, line);
return NULL;
#line 1819 "ifupdown.nw"
			}
		}
	}
}
#line 1831 "ifupdown.nw"
if (currif->n_options >= currif->max_options) {
	
#line 1853 "ifupdown.nw"
{
	variable *opt;
	currif->max_options = currif->max_options + 10;
	opt = realloc(currif->option, sizeof(*opt) * currif->max_options);
	if (opt == NULL) {
		
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1859 "ifupdown.nw"
	}
	currif->option = opt;
}
#line 1833 "ifupdown.nw"
}

currif->option[currif->n_options].name = strdup(firstword);
currif->option[currif->n_options].value = strdup(rest);

if (!currif->option[currif->n_options].name) {
	
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1840 "ifupdown.nw"
}

if (!currif->option[currif->n_options].value) {
	
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1844 "ifupdown.nw"
}

currif->n_options++;	
#line 1502 "ifupdown.nw"
		break;
	case MAPPING:
		
#line 1582 "ifupdown.nw"
if (strcmp(firstword, "script") == 0) {
	
#line 1592 "ifupdown.nw"
if (currmap->script != NULL) {
	
#line 1953 "ifupdown.nw"
fprintf(stderr, "%s:%d: duplicate script in mapping\n", filename, line);
return NULL;
#line 1594 "ifupdown.nw"
} else {
	currmap->script = strdup(rest);
}
#line 1584 "ifupdown.nw"
} else if (strcmp(firstword, "map") == 0) {
	
#line 1600 "ifupdown.nw"
if (currmap->max_mappings == currmap->n_mappings) {
	char **opt;
	currmap->max_mappings = currmap->max_mappings * 2 + 1;
	opt = realloc(currmap->mapping, sizeof(*opt) * currmap->max_mappings);
	if (opt == NULL) {
		
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1606 "ifupdown.nw"
	}
	currmap->mapping = opt;
}
currmap->mapping[currmap->n_mappings] = strdup(rest);
currmap->n_mappings++;
#line 1586 "ifupdown.nw"
} else {
	
#line 1958 "ifupdown.nw"
fprintf(stderr, "%s:%d: misplaced option\n", filename, line);
return NULL;
#line 1588 "ifupdown.nw"
}
#line 1505 "ifupdown.nw"
		break;
	case NONE:
	default:
		
#line 1958 "ifupdown.nw"
fprintf(stderr, "%s:%d: misplaced option\n", filename, line);
return NULL;
#line 1509 "ifupdown.nw"
}
#line 1495 "ifupdown.nw"
}
#line 1169 "ifupdown.nw"
	}
	if (
#line 1254 "ifupdown.nw"
ferror(f) != 0
#line 1170 "ifupdown.nw"
                                           ) {
		
#line 1913 "ifupdown.nw"
perror(filename);
return NULL;
#line 1172 "ifupdown.nw"
	}

	
#line 1208 "ifupdown.nw"
fclose(f);
line = -1;

#line 1176 "ifupdown.nw"
	return defn;
}
#line 1267 "ifupdown.nw"
static int get_line(char **result, size_t *result_len, FILE *f, int *line) {
	
#line 1292 "ifupdown.nw"
size_t pos;

#line 1270 "ifupdown.nw"
	do {
		
#line 1299 "ifupdown.nw"
pos = 0;
#line 1272 "ifupdown.nw"
		
#line 1310 "ifupdown.nw"
do {
	
#line 1331 "ifupdown.nw"
if (*result_len - pos < 10) {
	char *newstr = realloc(*result, *result_len * 2 + 80);
	if (newstr == NULL) {
		return 0;
	}
	*result = newstr;
	*result_len = *result_len * 2 + 80;
}
#line 1312 "ifupdown.nw"
	
#line 1360 "ifupdown.nw"
if (!fgets(*result + pos, *result_len - pos, f)) {
	if (ferror(f) == 0 && pos == 0) return 0;
	if (ferror(f) != 0) return 0;
}
pos += strlen(*result + pos);
#line 1313 "ifupdown.nw"
} while(
#line 1351 "ifupdown.nw"
pos == *result_len - 1 && (*result)[pos-1] != '\n'
#line 1313 "ifupdown.nw"
                                   );

#line 1372 "ifupdown.nw"
if (pos != 0 && (*result)[pos-1] == '\n') {
	(*result)[--pos] = '\0';
}

#line 1317 "ifupdown.nw"
(*line)++;

assert( (*result)[pos] == '\0' );
#line 1273 "ifupdown.nw"
		
#line 1388 "ifupdown.nw"
{ 
	int first = 0; 
	while (isspace((*result)[first]) && (*result)[first]) {
		first++;
	}

	memmove(*result, *result + first, pos - first + 1);
	pos -= first;
}
#line 1274 "ifupdown.nw"
	} while (
#line 1412 "ifupdown.nw"
(*result)[0] == '#'
#line 1274 "ifupdown.nw"
                               );

	while (
#line 1416 "ifupdown.nw"
(*result)[pos-1] == '\\'
#line 1276 "ifupdown.nw"
                               ) {
		
#line 1420 "ifupdown.nw"
(*result)[--pos] = '\0';
#line 1278 "ifupdown.nw"
		
#line 1310 "ifupdown.nw"
do {
	
#line 1331 "ifupdown.nw"
if (*result_len - pos < 10) {
	char *newstr = realloc(*result, *result_len * 2 + 80);
	if (newstr == NULL) {
		return 0;
	}
	*result = newstr;
	*result_len = *result_len * 2 + 80;
}
#line 1312 "ifupdown.nw"
	
#line 1360 "ifupdown.nw"
if (!fgets(*result + pos, *result_len - pos, f)) {
	if (ferror(f) == 0 && pos == 0) return 0;
	if (ferror(f) != 0) return 0;
}
pos += strlen(*result + pos);
#line 1313 "ifupdown.nw"
} while(
#line 1351 "ifupdown.nw"
pos == *result_len - 1 && (*result)[pos-1] != '\n'
#line 1313 "ifupdown.nw"
                                   );

#line 1372 "ifupdown.nw"
if (pos != 0 && (*result)[pos-1] == '\n') {
	(*result)[--pos] = '\0';
}

#line 1317 "ifupdown.nw"
(*line)++;

assert( (*result)[pos] == '\0' );
#line 1279 "ifupdown.nw"
	}

	
#line 1400 "ifupdown.nw"
while (isspace((*result)[pos-1])) { /* remove trailing whitespace */
	pos--;
}
(*result)[pos] = '\0';

#line 1283 "ifupdown.nw"
	return 1;
}
#line 1458 "ifupdown.nw"
static char *next_word(char *buf, char *word, int maxlen) {
	if (!buf) return NULL;
	if (!*buf) return NULL;

	while(!isspace(*buf) && *buf) {
		if (maxlen-- > 1) *word++ = *buf;
		buf++;
	}
	if (maxlen > 0) *word = '\0';

	while(isspace(*buf) && *buf) buf++;

	return buf;
}
#line 1704 "ifupdown.nw"
static address_family *get_address_family(address_family *af[], char *name) {
	int i;
	for (i = 0; af[i]; i++) {
		if (strcmp(af[i]->name, name) == 0) {
			return af[i];
		}
	}
	return NULL;
}
#line 1735 "ifupdown.nw"
static method *get_method(address_family *af, char *name) {
	int i;
	for (i = 0; i < af->n_methods; i++) {
		if (strcmp(af->method[i].name, name) == 0) {
			return &af->method[i];
		}
	}
	return NULL;
}
#line 1788 "ifupdown.nw"
static int duplicate_if(interface_defn *ifa, interface_defn *ifb) {
	if (strcmp(ifa->iface, ifb->iface) != 0) return 0;
	if (ifa->address_family != ifb->address_family) return 0;
	return 1;
}
