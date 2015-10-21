#include "OsdEngine.h"
#include "ReadOSD.h"

sub_data* SubRead(char *filename, float pts)
{
	return sub_read_file(filename,pts);
}

subtitle *SubFind(sub_data* subd,double pts)
{
	return find_sub_info(subd,pts);
}

void SubFree( sub_data * subd )
{
	sub_free(subd);
}

