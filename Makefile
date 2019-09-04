CC = gcc
EXTRA_CFLAGS = -Wall -Wextra

pwrm_sources = \
	main.c

pwrm_objects = $(pwrm_sources:.c=.o)

pwrm: $(pwrm_objects)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

clean:
	rm -fv \
	$(pwrm_objects) \
	pwrm
