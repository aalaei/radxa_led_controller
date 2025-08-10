#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef LED_PATH
#define LED_PATH "/sys/devices/platform/gpio-leds/leds/user-led2/brightness"
#endif
int led_fd;

void cleanup(int signum) {
    if (led_fd > 0) {
        close(led_fd);
        printf("LED file closed.\n");
    }
    exit(EXIT_SUCCESS);
}

void set_led(int state) {
    if (led_fd < 0) {
        fprintf(stderr, "LED file not open\n");
        return;
    }

    char buffer[2];
    snprintf(buffer, sizeof(buffer), "%d", state);
    write(led_fd, buffer, strlen(buffer));
}
int get_led(){
    if (led_fd < 0) {
        fprintf(stderr, "LED file not open\n");
        return -1;
    }
    char buffer[2];
    int state;
    lseek(led_fd, 0, SEEK_SET);
    read(led_fd, buffer, sizeof(buffer) - 1);
    buffer[1] = '\0'; // Null-terminate the buffer
    // printf(">>%s\n", buffer);
    sscanf(buffer, "%d", &state);
    return state;
}
void drop_privileges() {
    setgid(getgid());
    setuid(getuid());
}

int main(int argc, char *argv[]) {
    

    // Open LED file before dropping privileges
    led_fd = open(LED_PATH, O_RDWR);
    if (led_fd < 0) {
        perror("Failed to open LED file");
        exit(EXIT_FAILURE);
    }
    // Drop privileges after opening the LED file
    drop_privileges();
    if(argc<2){
        printf("%d", get_led());
        return -1;
    }
    int val=0;
    sscanf(argv[1], "%d",&val);
    if (val>0)
        set_led(1);
    else
        set_led(0);
    return 0;
}
