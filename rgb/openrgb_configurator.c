#include "../globals/globals.h"
#include "../parser/config.h"
#include "openrgb.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

void display_menu(bool selected[], int current_device) {
    for (int i = 0; i < openrgb_devices_num; i++) {
        printf("%s  [%c] Name: %s, Vendor: %s\n", (i == current_device) ? "->" : "  ", selected[i] ? 'x' : ' ',
               openrgb_controllers[i].name, openrgb_controllers[i].vendor);
    }
}

void save_selected_devices(bool selected[]) {
    struct stat st = {0};
    if (stat("/etc/piled", &st) == -1) {
        mkdir("/etc/piled", 0755);
    }
    FILE *file = fopen("/etc/piled/openrgb_config", "w");
    if (file == NULL) {
        perror("Failed to open file at /etc/piled/openrgb_config, please run configurator with root.");
        return;
    }

    for (int i = 0; i < openrgb_devices_num; i++) {
        if (selected[i]) {
            fprintf(file, "#%d: %s\n", i, openrgb_controllers[i].name);
        }
    }
    chmod("/etc/piled/openrgb_config", strtol("0644", 0, 8));
    fclose(file);

    printf("Selected devices saved to '/etc/piled/openrgb_config'.\n");
}

// https://stackoverflow.com/a/16361724
char getch(void) {
    char buf = 0;
    struct termios old = {0};
    fflush(stdout);
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return buf;
}

int main(int argc, char *argv[]) {
    if (load_config() != 0) {
        return -1;
    }

    parse_args(argc, argv);

    if (OPENRGB_SERVER) {
        openrgb_init();
    } else {
        logger("OpenRGB server ip not set! Aborting.");
        return -1;
    }

    bool selected[openrgb_devices_num];
    memset(selected, false, sizeof(selected));
    int current_device = 0;
    char input;

    while (1) {
        system("clear");
        printf("Please, choose which devices PiLED need to set color too:\n");
        display_menu(selected, current_device);

        printf("\nUse arrow keys to navigate, space to toggle, 'q' to quit and save.\n");

        input = getch();
        if (input == 'q') {
            break;
        } else if (input == ' ') {
            selected[current_device] = !selected[current_device];
        } else if (input == 'w' || input == 65) {
            current_device = (current_device - 1 + openrgb_devices_num) % openrgb_devices_num;
        } else if (input == 's' || input == 66) {
            current_device = (current_device + 1) % openrgb_devices_num;
        }
    }

    save_selected_devices(selected);

    openrgb_shutdown();
}