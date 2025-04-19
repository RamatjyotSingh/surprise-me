#ifndef SPINNER_H
#define SPINNER_H

#include <stdbool.h>

// Opaque spinner handle
typedef struct spinner spinner_t;

// Create a spinner that will show `msg…`
spinner_t *spinner_create(const char *msg);

// Start the spinner in a background thread
void spinner_start(spinner_t *s);

// Stop the spinner, print a green check or red X based on `success`
void spinner_stop(spinner_t *s, bool success);

// Clean up and free all resources
void spinner_destroy(spinner_t *s);

#endif // SPINNER_H