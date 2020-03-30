#include <ctype.h>
#include <fcntl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PATH "/sys/class/powercap/intel-rapl:1/energy_uj"
#define BUFFER_SIZE 80
#define DURATION_MIN 0.1
#define DURATION_MAX 60.
#define DEFAULT_DURATION 1.
#define COOL_DOWN 0.1
#define CONTINUOUS_ARG "--continuous"

void print_help() {
    printf(
        "Usage:\n  > pwrm [duration] [%s]\n\n"
        "Arguments:\n"
        "  [duration]\t\t\tMeasurement duration in seconds (from %.1f to %.1f)\n"
        "  %s\t\t\tPerform continuous measurements with a given interval\n"
        "  --help\t\t\tShow this message\n\n"
        "Return value:\n  Average power usage in Watts during given time period\n\n"
        "Examples:\n"
        "  > pwrm\t\t\tSingle measurement with default duration - %.2f sec\n"
        "  > pwrm %.1f\t\t\tSingle measurement with %.1f sec duration\n"
        "  > pwrm %s\t\tContinuous measurement with default intervals - %.2f sec\n"
        "  > pwrm %.1f %s\tContinuous measurement with %.1f sec intervals\n",
        CONTINUOUS_ARG,
        DURATION_MIN, DURATION_MAX,
        CONTINUOUS_ARG,
        DEFAULT_DURATION,
        1.5, 1.5,
        CONTINUOUS_ARG, DEFAULT_DURATION,
        1.5, CONTINUOUS_ARG, 1.5
    );
}

void print_unexpected_arg(char * arg) {
    printf("\033[0;31mUnexpected argument: %s\033[0m\n\n", arg);
    print_help();
}

void print_invalid_duration(char * duration_arg) {
    printf(
        "\033[0;31mInvalid duration value: %s\033[0m\n"
        "Duration must be in range from %.1f to %.1f\n\n",
        duration_arg, DURATION_MIN, DURATION_MAX
    );
    print_help();
}

double parse_duration(char * arg) {
    double duration = atof(arg);
    int i;
    
    if (!duration) {
        print_unexpected_arg(arg);
        return 0;
    }
    for (i = 0; arg[i] != '\0'; i++) {
        if (isdigit(arg[i]) == 0 && arg[i] != *localeconv()->decimal_point) {
            print_invalid_duration(arg);
            return 0;
        }
    }
    if (duration < DURATION_MIN || duration > DURATION_MAX) {
        print_invalid_duration(arg);
        return 0;
    }
    return duration;
}

void fill_timespec(struct timespec * timespec_var, float time) {
    timespec_var->tv_sec = (time_t) time;
    timespec_var->tv_nsec = (suseconds_t) ((time - timespec_var->tv_sec) * 1000000000.);
}

// TODO: implement automatic path retrieval
char * get_path() {
    return PATH;
}

int main(int argc, char ** argv) {
    double sleep = DEFAULT_DURATION;
    bool continuous_measurement = false;
    
    double power_used;
    int64_t power_value;
    int64_t last_power_value;
    
    int file_descriptor;
    char buffer[BUFFER_SIZE];
    int data_size;

    struct timespec sleep_spec;
    struct timespec cooldown_spec;
    struct timespec measure_time;
    struct timespec time_now;
    struct timespec time_diff;
    
    int64_t time_diff_nsec;
    
    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[1], CONTINUOUS_ARG) == 0) {
            continuous_measurement = true;
        } else {
            double arg = parse_duration(argv[1]);
            if (!arg) return 1;
            sleep = arg;
        }
    } else if (argc == 3) {
        double arg = parse_duration(argv[1]);
        if (!arg) return 1;
        sleep = arg;
        
        if (strcmp(argv[2], CONTINUOUS_ARG) == 0) {
            continuous_measurement = true;
        } else {
            print_unexpected_arg(argv[2]);
            return 1;
        }
    } else if (argc > 3) {
        printf("\033[0;31mInvalid number of arguments\033[0m\n\n");
        print_help();
        return 1;
    }
    
    fill_timespec(&cooldown_spec, COOL_DOWN);
    fill_timespec(&sleep_spec, sleep);

    file_descriptor = open(get_path(), O_RDONLY);
    if (file_descriptor < 0) return 1;

    nanosleep(&cooldown_spec, NULL);

    while (1) {
        lseek(file_descriptor, 0, SEEK_SET);
        data_size = read(file_descriptor, buffer, BUFFER_SIZE - 1);
        clock_gettime(CLOCK_MONOTONIC, &time_now);
        
        if (data_size > 0) {
            buffer[buffer[data_size - 1] == '\n' ? data_size - 1 : data_size] = '\0';
            power_value = (int64_t) atoll(buffer);
            
            if (last_power_value > 0 && power_value >= last_power_value) {
                time_diff.tv_sec = time_now.tv_sec - measure_time.tv_sec;
                time_diff.tv_nsec = time_now.tv_nsec - measure_time.tv_nsec;
                time_diff_nsec = (int64_t) (time_diff.tv_sec * 1000000000 + time_diff.tv_nsec);
                
                power_used = (double) (power_value - last_power_value) * 1000 / time_diff_nsec;
                printf("%4.02f\n", power_used);
                if (continuous_measurement == false) break;
            }
            
            last_power_value = power_value;
            measure_time = time_now;
        }
        
        nanosleep(&sleep_spec, NULL);
    }
    
    close(file_descriptor);
}
