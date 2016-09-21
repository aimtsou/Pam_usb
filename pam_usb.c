#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
#include <libusb.h>
#include <security/pam_modules.h>
#include <security/_pam_macros.h>

#define PAM_SB_AUTH
#define SERIAL_LENGTH 256

struct usb_device {
    int    vendor;
    int    product;
    char  *serial;
};

struct usb_user {
    char  *login;
    size_t ndevices;
    struct usb_device *devices;
};

struct usb_conf {
    size_t nusers;
    struct usb_user *users;
};

int read_config(struct usb_conf **conf, const char *config_file) {
    struct usb_conf *res;
    config_setting_t *users, *devices, *user, *device;
    config_t cfg;
    int i, j;
    char *msg = NULL;
    const char *tmp;

    config_init(&cfg);
    if(!config_read_file(&cfg, config_file)){
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return 0;
    }

    if((users = config_lookup(&cfg, "users")) == NULL){
        msg = "no users in config"; goto error;
    }

    int nusers = config_setting_length(users);
    res = malloc(sizeof(struct usb_conf));
    if(res == NULL){
        msg = "malloc for pam_usb_conf failed"; goto error;
    }
    res->nusers = nusers;

    res->users = malloc(nusers * sizeof(struct usb_user));
    for(i = 0; i < nusers; i++){
        if((user = config_setting_get_elem(users, i)) == NULL) continue;
        if((devices = config_setting_get_member(user, "devices")) == NULL){
            msg = "no devices for user"; goto error;
        }
        int ndevices = config_setting_length(devices);
        res->users[i].devices = malloc(ndevices * sizeof(struct usb_device));
        res->users[i].ndevices = ndevices;
        if(config_setting_lookup_string(user, "login", &tmp) == CONFIG_FALSE){
            msg = "no login name specified"; goto error;
        }
        res->users[i].login = strdup(tmp);
        for(j = 0; j < ndevices; j++){
            if((device = config_setting_get_elem(devices, j)) == NULL) continue;
            if(config_setting_lookup_int(device, "vid", &res->users[i].devices[j].vendor) == CONFIG_FALSE){
                msg = "no device vendor specified"; goto error;
            }
            if(config_setting_lookup_int(device, "pid", &res->users[i].devices[j].product) == CONFIG_FALSE){
                msg = "no device product specified"; goto error;
            }
            if(config_setting_lookup_string(device, "serial", &tmp) == CONFIG_FALSE){
                msg = "no device serial specified"; goto error;
            }
            res->users[i].devices[j].serial = strdup(tmp);
        }
    }
    config_destroy(&cfg);
    *conf = res;
    return 1;

error:
    if (msg) fprintf(stderr, "%s: %s", config_error_file(&cfg), msg);
    config_destroy(&cfg);
    return 0;
}

char *usb_device_get_serial(libusb_device *dev) {
    int res = 0;
    char *serial = NULL;
    libusb_device_handle *hdev = NULL;
    struct libusb_device_descriptor desc;

    if (libusb_open(dev, &hdev) != 0) return NULL;
    if (libusb_get_device_descriptor(dev, &desc) != 0) goto done;
    
    serial = malloc(SERIAL_LENGTH);
    if(serial == NULL){
        fprintf(stderr, "malloc for serial failed"); goto done;
    }
    res = libusb_get_string_descriptor_ascii(hdev, desc.iSerialNumber, (unsigned char *)serial, SERIAL_LENGTH);

    if (res <= 0){
        free(serial);
        serial = NULL;
    }

done:
    libusb_close(hdev);
    return serial;
}

int usb_device_match(struct usb_device *element, libusb_device *dev) {
    int   match  = 1;
    char *serial = NULL;
    struct libusb_device_descriptor desc;

    if (libusb_get_device_descriptor(dev, &desc) != 0){
        match = 0; return match;
    }

    serial = usb_device_get_serial(dev);
    if (element->serial && serial && strcmp(element->serial, serial)) match = 0;
    free(serial);
    if (element->vendor && element->vendor != desc.idVendor) match = 0;
    if (element->product && element->product != desc.idProduct) match = 0;

    return match;
}

int usb_device_find(struct usb_device *dev) {
    int retval = 0;
    int found = 0;
    ssize_t count = 0;
    size_t idx = 0;
    libusb_context *context = NULL;
    libusb_device **list = NULL;

    retval = libusb_init(&context);
    if (retval < 0){
       fprintf(stderr, "libusb_init failed");
       return found;
    }
    if (count = libusb_get_device_list(context, &list) > 0){
        for (idx = 0; idx < count; ++idx){
            if (usb_device_match(dev, list[idx])){
                found = 1;
                break;
            }
        }
        libusb_free_device_list(list, 1);
    }

    libusb_exit(context);
    return found;
}

int authenticate(struct usb_conf *cfg, const char *login) {
    int i;
    struct usb_user *user = NULL;

    for (i = 0; i < cfg->nusers; i++){
        if (!strcmp(cfg->users[i].login, login)){
            user = &cfg->users[i];
            break;
        }       
    }
    if (user){
        for (i = 0; i < user->ndevices; i++){
            struct usb_device *device = &user->devices[i];
            if (usb_device_find(device))
                return 1;
        }
    }
    return 0;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *hpam, int flags, int argc, const char **argv) {
    struct usb_conf *cfg;
    const char *user = NULL;
    int res = PAM_AUTH_ERR;

    if (pam_get_user(hpam, &user, NULL) != PAM_SUCCESS) return res;
    read_config(&cfg, "config.cfg");

    if (!authenticate(cfg, user))
       res = PAM_SUCCESS;
    else
        res = PAM_AUTH_ERR;

    free(cfg);
    return res;
}
