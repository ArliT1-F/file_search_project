#include "fs.h"
#include <string.h>
#include <stdio.h>

int main() {
    fs_init();

    char line[512];

    while (1) {
        printf("fs> ");
        if (!fgets(line, sizeof(line), stdin)) break;

        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, "list") == 0) {
            fs_list();
        }
        else if (strncmp(line, "create ", 7) == 0) {
            fs_create(line + 7);
        }
        else if (strncmp(line, "write ", 6) == 0) {
            char *p = strchr(line + 6, ' ');
            if (!p) continue;
            *p = 0;
            fs_write(line + 6, p + 1);
        }
        else if (strncmp(line, "read ", 5) == 0) {
            fs_read(line + 5);
        }
        else if (strncmp(line, "delete ", 7) == 0) {
            fs_delete(line + 7);
        }
        else if (strcmp(line, "exit") == 0) {
            break;
        }
        else {
            printf("Unknown command\n");
        }
    }

    return 0;
}