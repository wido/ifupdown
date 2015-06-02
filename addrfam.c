#include <stdlib.h>

#include "header.h"

address_family *addr_fams[] = {
	&addr_inet,
	&addr_inet6,
	&addr_ipx,
	&addr_can,
	&addr_meta,
	NULL
};
