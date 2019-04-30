#include "utils.h"

#include <sys/types.h>
#include <sys/stat.h>


/**
 * If the specified directory by param don't exist create it
 */
void mkdirIfNotExist(const char* directory)
{
  struct stat st = {0};

  if (stat(directory, &st) == -1) {
      mkdir(directory, 0700);
  }
}



/**
 * Initialize a matrix with 0s
 */
void initializeToZero(double* data, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
			data[i] = .0;
	}
}
