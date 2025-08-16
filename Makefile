CC = gcc
CFLAGS = -static -Wall
TARGET = led_tcp_control
LED_PATH ?= \"/sys/devices/platform/*leds/leds/*/brightness\"


.PHONY: venv

all: $(TARGET)

$(TARGET): led_control.c
	$(CC) $(CFLAGS) -DLED_PATH=$(LED_PATH) led_control.c -o $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/$(TARGET)
	sudo chown root:root /usr/local/bin/$(TARGET)
	sudo chmod u+s /usr/local/bin/$(TARGET)

install-service:
	sudo cp led_controller.service /etc/systemd/system/led_controller.service
	sudo systemctl daemon-reload
	sudo systemctl enable led_controller.service
	sudo systemctl start led_controller.service

venv:
	python3 -m venv env
	. env/bin/activate && pip install -r requirements.txt

setup: install venv install-service

clean:
	rm -f $(TARGET)
