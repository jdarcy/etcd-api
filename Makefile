OBJECTS	= etcd-api.o etcd-test.o
TARGET	= etcd-test
CFLAGS	= -DDEBUG -g -O0

TARGET: $(OBJECTS)
	$(CC) $(OBJECTS) -lcurl -lyajl -o $(TARGET)

clean:
	rm -f $(OBJECTS)

clobber distclean realclean spotless: clean
	rm -f $(TARGET)
