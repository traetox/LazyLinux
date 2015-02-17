CC=/usr/bin/gcc
CFLAGS=-Os -g
LDFLAGS=-lX11 -lXss -lXext

OUTPUT=LazyLinux
OBJ=main.o ssh.o
SRC=main.c ssh.c
HDR=ssh.h
INSTALL_DIR=/usr/bin/

$(OUTPUT): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(OUTPUT)

main.o: main.c
	$(CC) main.c $(CFLAGS) -c

ssh.o: ssh.c ssh.h
	$(CC) ssh.c $(CFLAGS) -c

clean:
	rm -f $(OBJ) $(OUTPUT)

install: $(OUTPUT) $(CONFIG)
	cp -f $(OUTPUT) $(INSTALL_DIR)
