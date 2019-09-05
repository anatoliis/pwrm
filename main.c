#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 80
#define PATH "/sys/class/powercap/intel-rapl:1/energy_uj"
#define DEFAULT_SLEEP 0.85
#define COOL_DOWN 0.15

struct measurement_t {
    float power;
    int64_t last_value;
    struct timespec measure_time;
};

void measure(struct measurement_t * measurement);

int main(int argc, char ** argv) {
    struct measurement_t * measurement = malloc(sizeof(struct measurement_t));

    struct timespec sleep_spec;
    struct timespec cooldown_spec;

    float sleep = DEFAULT_SLEEP;
    float arg_sleep;

    if (argc == 2) {
        arg_sleep = atof(argv[1]);
        if (!arg_sleep) {
            printf("%s\n", "Error: invalid argument");
            return 1;
        }
        if (arg_sleep > 5 || arg_sleep < 0.5) {
            printf("%s\n", "Error: argument must be in range from 0.5 to 5.0");
            return 1;
        }
        sleep = arg_sleep;
    } else if (argc > 2) {
        printf("%s\n", "Error: expected a single argument");
        return 1;
    }

    sleep -= COOL_DOWN;

    measurement->power = 0;
    measurement->last_value = 0;
    measurement->measure_time.tv_sec = 0;
    measurement->measure_time.tv_nsec = 0;

    sleep_spec.tv_sec = (time_t) sleep;
    sleep_spec.tv_nsec = (suseconds_t) ((sleep - sleep_spec.tv_sec)	* 1000000000.);
    cooldown_spec.tv_sec = (time_t) COOL_DOWN;
    cooldown_spec.tv_nsec = (suseconds_t) ((COOL_DOWN - cooldown_spec.tv_sec) * 1000000000.);

    nanosleep(&cooldown_spec, NULL);
    measure(measurement);
    nanosleep(&sleep_spec, NULL);
    measure(measurement);

    if (measurement->power == 0) {
        nanosleep(&sleep_spec, NULL);
        measure(measurement);
    }

    printf("%4.02f W\n", measurement->power);
}

void measure(struct measurement_t * measurement) {
    char buffer[BUFFER_SIZE];
    int file_descriptor = open(PATH, O_RDONLY);
    if (file_descriptor >= 0) {
        int size = read(file_descriptor, buffer, BUFFER_SIZE - 1);
        if (size > 0) {
            struct timespec time_now;
            double power = 0;
            buffer[buffer[size - 1] == '\n' ? size - 1 : size] = '\0';
            int64_t value = (int64_t) atoll(buffer);
            clock_gettime(CLOCK_MONOTONIC, &time_now);
            if (measurement->last_value > 0 && value >= measurement->last_value) {
                struct timespec time_diff;
                int64_t diff;
                time_diff.tv_sec = time_now.tv_sec - measurement->measure_time.tv_sec;
                time_diff.tv_nsec = time_now.tv_nsec - measurement->measure_time.tv_nsec;
                while (time_diff.tv_nsec < 0) {
                    time_diff.tv_nsec += 1000000000;
                    time_diff.tv_sec--;
                }
                diff = (int64_t) (time_diff.tv_sec * 1000000000 + time_diff.tv_nsec);
                power = (double) (value - measurement->last_value) * 1000 / diff;
            } else {
                power = measurement->power;
            }
            measurement->last_value = value;
            measurement->measure_time = time_now;
            measurement->power = power;
        }
        close(file_descriptor);
    }
}
