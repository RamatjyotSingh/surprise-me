#include "spinner.h"
#include "colors.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct spinner {
    char       *msg;
    const char *symbols;
    size_t      symcount;
    size_t      idx;
    bool        active;
    pthread_t   tid;
};

static void *spinner_thread(void *arg) {
    spinner_t *s = arg;
    // print initial message
    printf(ANSI_BOLD ANSI_BLUE "%s…" ANSI_RESET " ", s->msg);
    fflush(stdout);

    while (s->active) {
        char c = s->symbols[s->idx++ % s->symcount];
        printf("\b%c", c);
        fflush(stdout);
        usleep(100000); // 100 ms
    }
    return NULL;
}

spinner_t *spinner_create(const char *msg) {
    spinner_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->msg      = strdup(msg);
    s->symbols  = "|/-\\";
    s->symcount = strlen(s->symbols);
    s->idx      = 0;
    s->active   = false;
    return s;
}

void spinner_start(spinner_t *s) {
    if (!s) return;
    s->active = true;
    pthread_create(&s->tid, NULL, spinner_thread, s);
}

void spinner_stop(spinner_t *s, bool success) {
    if (!s) return;
    s->active = false;
    pthread_join(s->tid, NULL);
    // backspace over last spinner char and print result
    printf("\b");
    if (success) {
        printf(ANSI_BRIGHT_GREEN "✔" ANSI_RESET "\n");
    } else {
        printf(ANSI_BRIGHT_RED   "✖" ANSI_RESET "\n");
    }
    fflush(stdout);
}

void spinner_destroy(spinner_t *s) {
    if (!s) return;
    free(s->msg);
    free(s);
}