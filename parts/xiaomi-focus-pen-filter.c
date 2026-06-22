/*
 * Copyright (C) 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Grabs the M80p touchscreen events, injects real pressure from the
 * Xiaomi Focus Pen's HID reports, and exposes a combined uinput device.
 * Android's MultiTouchInputMapper will see proper pressure on every event.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* HID report */
#define REPORT_ID_DIGITIZER 0x05
#define REPORT_SIZE         15

/* M80p touchscreen parameters */
#define TS_X_MAX      21359
#define TS_Y_MAX      31999
#define PRESSURE_MAX  8191
#define TILT_MAX      60

static int uinput_fd = -1;
static int latest_pressure = 0;
static int pen_touching = 0;

/* Set up a uinput clone of the M80p with working pressure */
static void setup_uinput(void) {
    struct uinput_setup usetup;

    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) { perror("open /dev/uinput"); exit(1); }

    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_TOOL_PEN);
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_STYLUS);

    ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_X);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_PRESSURE);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_DISTANCE);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_TILT_X);
    ioctl(uinput_fd, UI_SET_ABSBIT, ABS_TILT_Y);

    struct uinput_abs_setup a;
    memset(&a, 0, sizeof(a));

    a.code = ABS_X; a.absinfo.minimum = 0; a.absinfo.maximum = TS_X_MAX; a.absinfo.resolution = 1;
    ioctl(uinput_fd, UI_ABS_SETUP, &a);
    a.code = ABS_Y; a.absinfo.minimum = 0; a.absinfo.maximum = TS_Y_MAX;
    ioctl(uinput_fd, UI_ABS_SETUP, &a);
    a.code = ABS_PRESSURE; a.absinfo.minimum = 0; a.absinfo.maximum = PRESSURE_MAX;
    ioctl(uinput_fd, UI_ABS_SETUP, &a);
    a.code = ABS_DISTANCE; a.absinfo.minimum = 0; a.absinfo.maximum = 1;
    ioctl(uinput_fd, UI_ABS_SETUP, &a);
    a.code = ABS_TILT_X; a.absinfo.minimum = -TILT_MAX; a.absinfo.maximum = TILT_MAX;
    ioctl(uinput_fd, UI_ABS_SETUP, &a);
    a.code = ABS_TILT_Y; a.absinfo.minimum = -TILT_MAX; a.absinfo.maximum = TILT_MAX;
    ioctl(uinput_fd, UI_ABS_SETUP, &a);

    ioctl(uinput_fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_I2C;
    usetup.id.vendor  = 0;
    usetup.id.product = 0;
    strncpy(usetup.name, "NVTCapacitivePenM80p-merged", UINPUT_MAX_NAME_SIZE - 1);

    if (ioctl(uinput_fd, UI_DEV_SETUP, &usetup) < 0) { perror("UI_DEV_SETUP"); exit(1); }
    if (ioctl(uinput_fd, UI_DEV_CREATE) < 0) { perror("UI_DEV_CREATE"); exit(1); }
    fprintf(stderr, "Created merged uinput device\n");
}

/* Find the Xiaomi Focus Pen hidraw device */
static int find_focus_pen_hidraw(void) {
    char path[64], buf[256];
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/sys/class/hidraw/hidraw%d/device/uevent", i);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) continue;
        buf[n] = '\0';
        if (strstr(buf, "Xiaomi Focus Pen") && !strstr(buf, "Keyboard") && !strstr(buf, "Mouse")) {
            snprintf(path, sizeof(path), "/dev/hidraw%d", i);
            return open(path, O_RDONLY | O_NONBLOCK);
        }
    }
    return -1;
}

/* Find an input event device by name from /proc/bus/input/devices */
static int find_event_by_name(const char *target_name) {
    FILE *f = fopen("/proc/bus/input/devices", "r");
    if (!f) return -1;

    char line[256];
    char name[256] = "";
    char handlers[256] = "";
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n') {
            /* End of device block */
            if (found) {
                char *p = strstr(handlers, "event");
                if (p) {
                    int num = atoi(p + 5);
                    fclose(f);
                    char path[64];
                    snprintf(path, sizeof(path), "/dev/input/event%d", num);
                    int efd = open(path, O_RDONLY);
                    if (efd >= 0) {
                        if (ioctl(efd, EVIOCGRAB, (void *)1) < 0)
                            fprintf(stderr, "WARNING: EVIOCGRAB on %s failed\n", path);
                        return efd;
                    }
                }
                return -1;
            }
            name[0] = '\0';
            handlers[0] = '\0';
            found = 0;
            continue;
        }
        if (strncmp(line, "N: Name=\"", 9) == 0) {
            size_t len = strlen(line);
            if (len > 10 && line[len-2] == '"') {
                len -= 2; /* trim trailing quote and newline */
                memcpy(name, line + 9, len - 9);
                name[len - 9] = '\0';
                if (strcmp(name, target_name) == 0) found = 1;
            }
        } else if (strncmp(line, "H: Handlers=", 12) == 0) {
            size_t len = strlen(line);
            if (len > 13) {
                memcpy(handlers, line + 12, len - 12);
                handlers[len - 13] = '\0'; /* trim newline */
            }
        }
    }
    fclose(f);
    return -1;
}

int main(void) {
    uint8_t hid_buf[REPORT_SIZE];
    struct input_event ev;
    int ts_fd = -1, pen_fd = -1;
    int pen_active = 0;
    int last_injected_pressure = -1;
    int reconnect_timer = 0;

    setup_uinput();

    /* Wait for both devices initially */
    fprintf(stderr, "Waiting for M80p touchscreen...\n");
    while (ts_fd < 0) { sleep(1); ts_fd = find_event_by_name("NVTCapacitivePenM80p"); }
    fprintf(stderr, "Waiting for Focus Pen...\n");
    while (pen_fd < 0) { sleep(1); pen_fd = find_focus_pen_hidraw(); }
    fprintf(stderr, "Both devices ready, forwarding events\n");

    struct pollfd fds[2];
    fds[0].fd = ts_fd;  fds[0].events = POLLIN;
    fds[1].fd = pen_fd; fds[1].events = POLLIN;

    while (1) {
        /* Try to reconnect pen if disconnected, at most once per second */
        if (pen_fd < 0 && reconnect_timer++ >= 4) {
            reconnect_timer = 0;
            pen_fd = find_focus_pen_hidraw();
            if (pen_fd >= 0) {
                fprintf(stderr, "Focus Pen reconnected\n");
                fds[1].fd = pen_fd;
                fds[1].revents = 0;
            }
        }

        int timeout = (pen_fd < 0) ? 250 : -1;
        int ret = poll(fds, 2, timeout);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        /* Process Focus Pen HID reports (just extract pressure) */
        if (fds[1].fd >= 0 && (fds[1].revents & (POLLIN | POLLERR | POLLHUP))) {
            ssize_t n = read(pen_fd, hid_buf, sizeof(hid_buf));
            if (n == REPORT_SIZE && hid_buf[0] == REPORT_ID_DIGITIZER) {
                int raw = (int16_t)(hid_buf[1] | (hid_buf[2] << 8));
                if (raw < 0) raw = 0;
                if (raw > PRESSURE_MAX) raw = PRESSURE_MAX;
                latest_pressure = raw;
                pen_touching = (raw > 0);
            } else {
                /* Pen disconnected — close, main loop will reconnect */
                fprintf(stderr, "Focus Pen disconnected\n");
                close(pen_fd);
                pen_fd = -1;
                fds[1].fd = -1;
                latest_pressure = 0;
                pen_touching = 0;
                reconnect_timer = 0;
            }
        }

        /* Process M80p touchscreen events */
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP)) {
            ssize_t n = read(ts_fd, &ev, sizeof(ev));
            if (n != sizeof(ev)) {
                close(ts_fd);
                ts_fd = -1;
                fds[0].fd = -1;
                fprintf(stderr, "M80p touchscreen gone, waiting...\n");
                while (ts_fd < 0) { sleep(1); ts_fd = find_event_by_name("NVTCapacitivePenM80p"); }
                fprintf(stderr, "M80p touchscreen back\n");
                fds[0].fd = ts_fd;
                fds[0].revents = 0;
                continue;
            }

            /* Drop the original ABS_PRESSURE and ABS_DISTANCE from the M80p
             * — we inject our own from the Focus Pen. */
            if (ev.type == EV_ABS && (ev.code == ABS_PRESSURE || ev.code == ABS_DISTANCE))
                continue;

            /* Track pen active state from BTN_TOOL_PEN (the M80p uses
             * this instead of BTN_TOUCH for pen input). */
            if (ev.type == EV_KEY && ev.code == BTN_TOOL_PEN) {
                pen_active = ev.value;
                if (!pen_active) {
                    last_injected_pressure = -1;
                    latest_pressure = 0;
                    pen_touching = 0;
                }
            }

            /* Inject ABS_PRESSURE before every SYN_REPORT when pen is active.
             * The original M80p never emits ABS_PRESSURE, so we inject it.
             * When Focus Pen reports real pressure (>0), use that.
             * Otherwise use a minimum of 1 for "0g" zero-force detection. */
            if (ev.type == EV_SYN && ev.code == SYN_REPORT && pen_active) {
                int p;
                if (pen_touching && latest_pressure > 0)
                    p = latest_pressure;
                else
                    p = 1; /* 0g: pen near screen, emit minimal pressure */
                if (p != last_injected_pressure) {
                    struct input_event pev;
                    memset(&pev, 0, sizeof(pev));
                    pev.type  = EV_ABS;
                    pev.code  = ABS_PRESSURE;
                    pev.value = p;
                    gettimeofday(&pev.time, NULL);
                    if (write(uinput_fd, &pev, sizeof(pev)) < 0)
                        perror("write pressure");
                    last_injected_pressure = p;
                }
                /* Also inject ABS_DISTANCE */
                {
                    struct input_event dev;
                    memset(&dev, 0, sizeof(dev));
                    dev.type  = EV_ABS;
                    dev.code  = ABS_DISTANCE;
                    dev.value = pen_touching ? 1 : 0;
                    gettimeofday(&dev.time, NULL);
                    if (write(uinput_fd, &dev, sizeof(dev)) < 0)
                        perror("write distance");
                }
            }

            if (write(uinput_fd, &ev, sizeof(ev)) < 0)
                perror("write uinput");
        }
    }

    close(uinput_fd);
    return 0;
}
