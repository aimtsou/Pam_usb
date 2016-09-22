# Pam_usb
PAM Module to authenticate users with common USB devices. It provides a two factor authentication by checking that a registered usb key 
is plugged into the system. It can be used with any pam-aware service. The PAM Module can be combined with the standard PAM-based password authentication.
This PAM module is written mostly for educational purposes. It needs some improvements if it has to be used in production environments.

# Requirements
You need the following packages installed in order to properly build, install and use this module:
- Redhat/CentOS/SLC5-6/CC7 (rpm):
  * Required: pam-devel
  * Configuration : libconfig-devel
  * Usb support : libusbx-devel

# Building
gcc -I/usr/include/libusb-1.0/ -o pam_usb.so pam_usb.c -lusb-1.0 -lconfig -fPIC -c
 
# Configuration
In order to use this PAM module you need to configure your PAM setup by adding a line to an appropriate file in /etc/pam.d/

# TODO
- [x] Per usb auth
- [ ] Add encryption key to mbr
- [ ] Add sms authentication
- [ ] Add TOTP/HOTP authentication through Authy / Google Authenticator
- [ ] Yubikey support
