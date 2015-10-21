#ifndef __OSDENGINE_H
#define __OSDENGINE_H

#include "ReadOSD.h"

# ifdef __cplusplus
extern "C" {
# endif

sub_data *SubRead(char *filename, float pts);
subtitle *SubFind(sub_data* subd,double pts);
void SubFree( sub_data * subd );

# ifdef __cplusplus
}
# endif 

#endif

