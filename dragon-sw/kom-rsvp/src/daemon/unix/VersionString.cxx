#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* VersionString() {
	return "Release 3.1pre - " BUILD_DATE;
}

/*** To automate version string generation from SVN, CHANGE HERE into ANYTHING before each 'commit' --> "06/21/2010" ***/

const char* DragonVersionString() {
    static char dvString[100];
    char* revString = (char*)"$Rev$";
    char* dateString = (char*)"$LastChangedDate$";
    sprintf(dvString, "Code Revision: %s", revString+strlen("$Rev: "));
    sprintf(dvString + strlen(dvString) - 2, " - Modification Date: %s", dateString+strlen("$LastChangedDate: "));
    dvString[strlen(dvString) - 2] = '\n';
    dvString[strlen(dvString) - 1] = '\000';
    return dvString;
}

