CC=/usr/bin/gcc
CFLAGS=-Os -g
LDFLAGS=-lX11 -lXss -lXext

OUTPUT=LazyLinux
OBJ=main.o ssh.o xstuff.o log.o
SRC=main.c ssh.c xstuff.c log.c
HDR=ssh.h xstuff.h log.h
INSTALL_DIR=/usr/bin/

$(OUTPUT): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(OUTPUT)

main.o: main.c
	$(CC) main.c $(CFLAGS) -c

ssh.o: ssh.c ssh.h
	$(CC) ssh.c $(CFLAGS) -c

log.o: log.h log.c
	$(CC) log.c $(CFLAGS) -c

clean:
	rm -f $(OBJ) $(OUTPUT)

install: $(OUTPUT) $(CONFIG)
	cp -f $(OUTPUT) $(INSTALL_DIR)
	chown root:root $(INSTALL_DIR)$(OUTPUT)
	chmod 0555 $(INSTALL_DIR)$(OUTPUT)
	chmod +s $(INSTALL_DIR)$(OUTPUT)
