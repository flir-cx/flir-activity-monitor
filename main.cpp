#include <iostream>

#include <linux/input.h>
#include <libevdev/libevdev.h>

#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

//logging
#include <syslog.h>

/* signal handling */
static sig_atomic_t abort_signal = 0;
static void signal_handler(int sig) {
    abort_signal = 1;
}

//static int is_event_device(const struct dirent *dir) {
//    return strncmp("event", dir->d_name, 5) == 0;
//}
//
//static int open_all_event_devices(struct pollfd *pfds, int npfds) {
//    struct dirent **event_devices;
//
//    int nbr_dev = scandir("/dev/input", &event_devices, is_event_device, alphasort);
//    if (nbr_dev <= 0) {
//        syslog(LOG_ERR, "Couldn't find any event devices at all in /dev/input.");
//        return -1;
//    }
//
//    int pfdi = 0;
//    for (int i = 0; i < nbr_dev; ++i) {
//        char full_path[320];
//        char name[256] = "???";
//        int fd = -1;
//
//        snprintf(full_path, sizeof(full_path), "/dev/input/%s", event_devices[i]->d_name);
//        if (pfdi >= npfds) {
//            syslog(LOG_ERR, "Could not add event device '%s' for monitoring, too many devices already added. (limit: %d)",
//                    full_path, npfds);
//            continue;
//        }
//        fd = open(full_path, O_RDONLY);
//        if (fd < 0) {
//            syslog(LOG_ERR, "Failed to open event device '%s' for monitoring (ret: %d, errno: %d).",
//                    full_path, fd, errno);
//            free(event_devices[i]);
//            continue;
//        }
//        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
//        syslog(LOG_INFO, "Adding event device '%s' with name '%s' for monitoring.", full_path, name);
//        pfds[pfdi].fd = fd;
//        pfdi++;
//
//        free(event_devices[i]);
//    }
//
//    return pfdi;
//}

int main(int argc, char *argv[]) {
    /* Enable breaking loop */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct libevdev *dev = NULL;
    int rc = 1;
    int fd = open("/dev/input/event1", O_RDONLY|O_NONBLOCK);
    rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
        exit(1);
    }
    do {
        struct input_event ev;
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == 0)
            printf("Event: %s %s %d\n",
                    libevdev_event_type_get_name(ev.type),
                    libevdev_event_code_get_name(ev.type, ev.code),
                    ev.value);
    } while ((rc == 1 || rc == 0 || rc == -EAGAIN) && !abort_signal);


//    struct pollfd pfds[16];
//    int ndevs = open_all_event_devices(pfds, 16);
//    printf("Opened: %d\n", ndevs);
//
//    while(!abort_signal) {
//        int prc = poll(pfds, ndevs, 50);
//        if (abort_signal)
//            break;
//
//        /* Got button event */
//        if (prc > 0) {
//            for (int evi = 0; evi < ndevs; ++evi) {
//                //if (pfds[evi].revents & POLLIN) {
//                if (pfds[evi].revents & ~0x104) {
//                    printf("Got event: 0x%X\n", pfds[evi].revents);
//                    if (pfds[evi].revents & POLLIN) {
//                        struct input_event ev[64];
//                        int rd = read(pfds[evi].fd, ev, sizeof(ev));
//                        for (int i = 0; i < rd / sizeof(struct input_event); ++i) {
//                            printf("Got event, t: %d, c: %d, v: %d\n", ev[i].type, ev[i].code, ev[i].value);
//                        }
//                        //pfds[evi].revents &= ~POLLIN;
//                    }
//                    pfds[evi].revents = 0;
//                }
//            }
//        }
//    }
    return 0;
}
