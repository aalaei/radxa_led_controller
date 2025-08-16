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

int get_led_state(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open LED file for reading");
        return -1;
    }
    char buffer[2];
    int state;
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0) {
        return -1;
    }
    buffer[bytes_read] = '\0';
    sscanf(buffer, "%d", &state);
    return state;
}

void set_led_state(const char *path, int state) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open LED file for writing");
        return;
    }
    char buffer[2];
    snprintf(buffer, sizeof(buffer), "%d", state);
    write(fd, buffer, strlen(buffer));
    close(fd);
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
        char *brightness_dir = dirname(path_copy); // .../leds/radxa:blue:user
        char *led_name = basename(brightness_dir); // radxa:blue:user
        
        printf("%s\n", led_name);
        free(path_copy);
    }
    globfree(&gl);
}

int main(int argc, char *argv[]) {
    drop_privileges();

    glob_t gl;
    int ret = glob(LED_BASE_PATH, GLOB_NOCHECK, NULL, &gl);
    if (ret != 0 || gl.gl_pathc == 0) {
        fprintf(stderr, "No LEDs found with the specified path pattern.\n");
        globfree(&gl);
        return EXIT_FAILURE;
    }
    
    qsort(gl.gl_pathv, gl.gl_pathc, sizeof(char *), (int(*)(const void *, const void *))strcmp);

    if (argc < 2) {
        const char *default_led_path = gl.gl_pathv[0];
        int state = get_led_state(default_led_path);
        if (state != -1) {
            printf("%d\n", state);
        }
        globfree(&gl);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--list") == 0) {
        list_leds();
        globfree(&gl);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--name") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --name <led_name> [state]\n", argv[0]);
            globfree(&gl);
            return EXIT_FAILURE;
        }

        const char *led_name = argv[2];
        char path_pattern[256];
        snprintf(path_pattern, sizeof(path_pattern), "/sys/devices/platform/*leds/leds/%s/brightness", led_name);

        glob_t gl_name;
        int ret_name = glob(path_pattern, GLOB_NOCHECK, NULL, &gl_name);
        if (ret_name != 0 || gl_name.gl_pathc == 0) {
            fprintf(stderr, "LED '%s' not found.\n", led_name);
            globfree(&gl);
            globfree(&gl_name);
            return EXIT_FAILURE;
        }

        const char *led_path = gl_name.gl_pathv[0];

        if (argc == 3) {
            int state = get_led_state(led_path);
            if (state != -1) {
                printf("%d\n", state);
            }
        } else if (argc == 4) {
            int val = atoi(argv[3]);
            set_led_state(led_path, val > 0 ? 1 : 0);
        } else {
            fprintf(stderr, "Usage: %s --name <led_name> [state]\n", argv[0]);
            globfree(&gl);
            globfree(&gl_name);
            return EXIT_FAILURE;
        }

        globfree(&gl_name);
        globfree(&gl);
        return EXIT_SUCCESS;
    }
    
    const char *default_led_path = gl.gl_pathv[0];
    int val = atoi(argv[1]);
    set_led_state(default_led_path, val > 0 ? 1 : 0);
    
    globfree(&gl);
    return EXIT_SUCCESS;
}