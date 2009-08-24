#include <stdio.h>
#include <stdlib.h>
#include <io.h>

int main()
{
	fprintf(stdout, "Hello, stdout!\n");
	fprintf(stderr, "Hello, stderr!\n");
	if (!isatty(0) || !isatty(1))
	{
		fprintf(stderr, "isatty(0) = %d\nisatty(1) = %d\n",
			isatty(0), isatty(1));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
