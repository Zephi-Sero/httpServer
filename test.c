#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main()
{
	char *const txt = malloc(128 + 1);
	txt[0] = '\0';
	strncat(txt, "hello!", 128);
	strncat(txt, "b", 128);
	printf("%s\n", txt);
	return 0;
}
