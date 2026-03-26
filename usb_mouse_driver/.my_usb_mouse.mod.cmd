savedcmd_my_usb_mouse.mod := printf '%s\n'   my_usb_mouse.o | awk '!x[$$0]++ { print("./"$$0) }' > my_usb_mouse.mod
