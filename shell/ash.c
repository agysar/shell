int ash_argc;
char **ash_argv;

#include "lib_ash.c"

int main (int argc, char **argv)
{
	ash_argc = argc;
	ash_argv = argv;
	
	clearWindow();
	
    ashLoop();
	return 0;
}
