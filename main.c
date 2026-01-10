#include "fs.h"
#include <stdio.h>
#include <string.h>

int main()
{
    fs_init();

    char line[256];

    while (1)
    {
        printf("fs> ");
        if (!fgets(line, sizeof(line), stdin))
            break;

        line[strcspn(line, "\n")] = 0;

        if (strncmp(line, "create ", 7) == 0)
        {
            fs_create(line + 7);
        }
        else if (strcmp(line, "exit") == 0)
        {
            break;
        }
        else
        {
            printf("Komanda e vetme qe suportohet: create <filename>\n");
        }
    }

    return 0;
}