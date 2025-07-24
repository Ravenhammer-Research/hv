CC=		cc
CFLAGS=		-Wall -Wextra -std=c99 -D_BSD_SOURCE -I/usr/local/include -I ./3rdparty/include
LDFLAGS=	-L/usr/local/lib -lutil -lreadline -lexpat -lzfs -lnvpair

# Build targets
all: hvd hv

hvd: hvd.o zfs_manager.o xml_config.o netd_integration.o vm_manager.o network_manager.o commands.o
	$(CC) -o hvd hvd.o zfs_manager.o xml_config.o netd_integration.o vm_manager.o network_manager.o commands.o $(LDFLAGS)

hv: hv_client.o
	$(CC) -o hv hv_client.o $(LDFLAGS)

# Object files
hvd.o: src/hvd.c src/common.h
	$(CC) $(CFLAGS) -c src/hvd.c

hv_client.o: src/hv_client.c src/common.h
	$(CC) $(CFLAGS) -c src/hv_client.c

zfs_manager.o: src/zfs_manager.c src/common.h
	$(CC) $(CFLAGS) -c src/zfs_manager.c

xml_config.o: src/xml_config.c src/common.h
	$(CC) $(CFLAGS) -c src/xml_config.c

netd_integration.o: src/netd_integration.c src/common.h
	$(CC) $(CFLAGS) -c src/netd_integration.c

vm_manager.o: src/vm_manager.c src/common.h
	$(CC) $(CFLAGS) -c src/vm_manager.c

network_manager.o: src/network_manager.c src/common.h
	$(CC) $(CFLAGS) -c src/network_manager.c

commands.o: src/commands.c src/common.h
	$(CC) $(CFLAGS) -c src/commands.c

clean:
	rm -f *.o hvd hv

install: hvd hv
	install -m 755 hvd /usr/local/sbin/
	install -m 755 hv /usr/local/bin/
	install -d /usr/local/share/yang
	install -m 644 yang/hvd-vm.yang /usr/local/share/yang/
	install -m 644 yang/hvd-network.yang /usr/local/share/yang/

uninstall:
	rm -f /usr/local/sbin/hvd
	rm -f /usr/local/bin/hv
	rm -f /usr/local/share/yang/hvd-vm.yang
	rm -f /usr/local/share/yang/hvd-network.yang

.PHONY: all clean install uninstall 
