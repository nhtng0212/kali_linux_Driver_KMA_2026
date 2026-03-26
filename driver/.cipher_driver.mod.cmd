savedcmd_cipher_driver.mod := printf '%s\n'   cipher_driver.o | awk '!x[$$0]++ { print("./"$$0) }' > cipher_driver.mod
