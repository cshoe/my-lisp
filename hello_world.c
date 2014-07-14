#include <stdio.h>


void print_hello_world(int num_times);


int main(int argc, char** argv) {
    puts("BUTTS");

    int i = 5;
    while (i > 0) {
        puts("While LOOPIN");
        i = i - 1;
    }

    for (int i = 0; i < 5; i++) {
        puts("for LOOPIN");
    }

    print_hello_world(5);
}


void print_hello_world(int num_times) {
    for (int i = 0; i < num_times; i++) {
        puts("FUNCTION printing");
    }
    return;
}
