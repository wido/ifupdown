#line 453 "ifupdown.nw"
#include <stdlib.h>
#include "header.h"

#line 3693 "ifupdown.nw"
extern address_family addr_inet;
#line 3873 "ifupdown.nw"
extern address_family addr_inet6;
#line 3944 "ifupdown.nw"
extern address_family addr_ipx;

#line 458 "ifupdown.nw"
address_family *addr_fams[] = {
	
#line 3697 "ifupdown.nw"
&addr_inet, 
#line 3877 "ifupdown.nw"
&addr_inet6,
#line 3948 "ifupdown.nw"
&addr_ipx,
#line 460 "ifupdown.nw"
	NULL
};
