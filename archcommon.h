#include "header.h"

bool execable(const char *);

#define iface_is_link() (!_iface_has(ifd->real_iface, ":."))
#define iface_has(s) _iface_has(ifd->real_iface, (s))
#define iface_is_lo() ((!strcmp(ifd->logical_iface, LO_IFACE)) && (!no_loopback))

bool _iface_has(const char *, const char *);
void cleanup_hwaddress(interface_defn *ifd, char **pparam, int argc, char **argv);
void make_hex_address(interface_defn *ifd, char **pparam, int argc, char **argv);
void compute_v4_addr(interface_defn *ifd, char **pparam, int argc, char **argv);
void compute_v4_mask(interface_defn *ifd, char **pparam, int argc, char **argv);
void compute_v4_broadcast(interface_defn *ifd, char **pparam, int argc, char **argv);
void set_preferred_lft(interface_defn *ifd, char **pparam, int argc, char **argv);
void get_token(interface_defn *ifd, char **pparam, int argc, char **argv);
void to_decimal(interface_defn *ifd, char **pparam, int argc, char **argv);
void map_value(interface_defn *ifd, char **pparam, int argc, char **argv);
void if_set(interface_defn *ifd, char **pparam, int argc, char **argv);
