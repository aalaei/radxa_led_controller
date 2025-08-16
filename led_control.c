#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <glob.h>

#ifndef LED_BASE_PATH
#define LED_BASE_PATH "/sys/devices/platform/*leds/leds/*/brightness"
#endif

void drop_privileges() {
    setgid(getgid());
    setuid(getuid());
}

int get_led_state(int fd) {
    if (fd < 0) {
        fprintf(stderr, "LED file not open\n");
        return -1;
    }
    char buffer[2];
    int state;
    lseek(fd, 0, SEEK_SET);
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        return -1;
    }
    buffer[bytes_read] = '\0';
    sscanf(buffer, "%d", &state);
    return state;
}

void set_led_state(int fd, int state) {
    if (fd < 0) {
        fprintf(stderr, "LED file not open\n");
        return;
    }
    char buffer[2];
    snprintf(buffer, sizeof(buffer), "%d", state);
    write(fd, buffer, strlen(buffer));
}

void list_leds() {
    glob_t gl;
    int ret = glob(LED_BASE_PATH, GLOB_NOCHECK, NULL, &gl);
    if (ret != 0) {
        perror("Glob failed");
        return;
    }

    for (size_t i = 0; i < gl.gl_pathc; ++i) {
        char *path_copy = strdup(gl.gl_pathv[i]);
        if (path_copy == NULL) continue;
        
        char *brightness_dir = dirname(path_copy);
        char *led_name = basename(brightness_dir);
        
        printf("%s\n", led_name);
        free(path_copy);
    }
    globfree(&gl);
}

int main(int argc, char *argv[]) {
    // List available LEDs before dropping privileges
    if (argc > 1 && strcmp(argv[1], "--list") == 0) {
        list_leds();
        return EXIT_SUCCESS;
    }

    glob_t gl;
    int ret = glob(LED_BASE_PATH, GLOB_NOCHECK, NULL, &gl);
    if (ret != 0 || gl.gl_pathc == 0) {
        fprintf(stderr, "No LEDs found with the specified path pattern.\n");
        globfree(&gl);
        return EXIT_FAILURE;
    }

    const char *led_path_to_open = NULL;
    char *target_led_name = NULL;

    if (argc < 2) {
        // Default to the first LED found
        led_path_to_open = gl.gl_pathv[0];
    } else if (strcmp(argv[1], "--name") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --name <led_name> [state]\n", argv[0]);
            globfree(&gl);
            return EXIT_FAILURE;
        }
        
        target_led_name = argv[2];
        for (size_t i = 0; i < gl.gl_pathc; i++) {
            char *path_copy = strdup(gl.gl_pathv[i]);
            char *brightness_dir = dirname(path_copy);
            char *led_name = basename(brightness_dir);
            
            if (strcmp(led_name, target_led_name) == 0) {
                led_path_to_open = gl.gl_pathv[i];
                free(path_copy);
                break;
            }
            free(path_copy);
        }

        if (led_path_to_open == NULL) {
            fprintf(stderr, "LED '%s' not found.\n", target_led_name);
            globfree(&gl);
            return EXIT_FAILURE;
        }

    } else {
        // Default behavior for direct state change
        led_path_to_open = gl.gl_pathv[0];
    }

    // Now, open the determined file path
    int led_fd = open(led_path_to_open, O_RDWR);
    if (led_fd < 0) {
        perror("Failed to open LED file");
        globfree(&gl);
        return EXIT_FAILURE;
    }

    // Drop privileges after the file is successfully opened
    drop_privileges();

    // Now, perform read or write operations using the opened file descriptor
    if (target_led_name) { // if the --name argument was used
        if (argc == 3) {
            int state = get_led_state(led_fd);
            if (state != -1) {
                printf("%d\n", state);
            }
        } else if (argc == 4) {
            int val = atoi(argv[3]);
            set_led_state(led_fd, val > 0 ? 1 : 0);
        }
    } else { // default or single argument case
        if (argc < 2) {
            int state = get_led_state(led_fd);
            if (state != -1) {
                printf("%d\n", state);
            }
        } else {
            int val = atoi(argv[1]);
            set_led_state(led_fd, val > 0 ? 1 : 0);
        }
    }

    close(led_fd);
    globfree(&gl);
    return EXIT_SUCCESS;
}