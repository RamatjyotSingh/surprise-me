#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <ftw.h>
#include <linux/limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include "err.h"
#include "spinner.h"

#define ASSETS_DIR "assets"
#define AUDIO_DIR "assets/audio"
#define ASCII_DIR "assets/ascii"
#define FRAMES_DIR "assets/frames"

#define DEFAULT_FPS  "10"
#define DEFAULT_WIDTH  "900"    // Doubled from 80
#define DEFAULT_HEIGHT "40"     // Doubled from 40
#define DEFAULT_START_FRAME "1"
#define DEFAULT_VIDEO_PATH "rr.mp4"

#define BUFFER_SIZE 1024

char VIDEO_PATH[PATH_MAX] = DEFAULT_VIDEO_PATH;
char* FPS = DEFAULT_FPS;
char* WIDTH = DEFAULT_WIDTH;
char* HEIGHT = DEFAULT_HEIGHT;
char* START_FRAME = DEFAULT_START_FRAME;

void extract_images_grayscale();
char* get_usage_msg(const char *program_name);
void create_dir(const char *dir_name);
void empty_directory(const char *dir_name);
void setup();
void play();
void draw_frames();
void draw_ascii_frame(const char *frame_path);
void batch_convert_to_ascii();
void extract_audio();
void play_audio();
int directory_exists(const char *path);
int is_directory_empty(const char *dir_path);

static void print_usage(const char *prog) {
    fprintf(stderr,
      "Usage: %s [OPTIONS]\n"
      "  -i, --input   PATH    input video path (optional)\n"
      "  -f, --fps     NUM     frames per second (default %s)\n"
      "  -w, --width   NUM     output width (default %s)\n"
      "  -t, --height  NUM     output height (default %s)\n"
      "  -s, --start   NUM     start frame (default %s)\n"
      "  -h, --help            this message\n",
      prog, DEFAULT_FPS, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_START_FRAME);
}

int main(int argc, char *argv[])
{
    int c, optidx = 0;
    int opts_given = 0;    // ← counter for any options

    static struct option longopts[] = {
        { "input",  required_argument, 0, 'i' },
        { "fps",    required_argument, 0, 'f' },
        { "width",  required_argument, 0, 'w' },
        { "height", required_argument, 0, 't' },
        { "start",  required_argument, 0, 's' },
        {"reset",  no_argument,       0, 'r' },
        { "help",   no_argument,       0, 'h' },
        { 0,0,0,0 }
    };

    while ((c = getopt_long(argc, argv, "i:f:w:t:s:h:r", longopts, &optidx)) != -1) {
        switch (c) {
          case 'i':
            opts_given++;
            strncpy(VIDEO_PATH, optarg, sizeof(VIDEO_PATH)-1);
            break;
          case 'f':
            opts_given++;
            FPS = optarg;
            break;
          case 'w':
            opts_given++;
            WIDTH = optarg;
            break;
          case 't':
            opts_given++;
            HEIGHT = optarg;
            break;
          case 's':
            opts_given++;
            START_FRAME = optarg;
            break;
          case 'r':
            opts_given++;

            break;
          case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
          default:
            exit(EXIT_FAILURE);
        }
    }

    if (opts_given != 0) {

        setup();

    }

    play();
    return EXIT_SUCCESS;
}

void setup(){
    if(VIDEO_PATH[0] == '\0') {
      fatal_error("Invalid path provided for video extraction, path is NULL");
    }
   create_dir(ASSETS_DIR);
   create_dir(ASCII_DIR);
   create_dir(AUDIO_DIR);
   create_dir(FRAMES_DIR);

   extract_audio();
    extract_images_grayscale();
    batch_convert_to_ascii();

}
void play_audio(){
    // redirect ffplay output so it doesn't write into your console
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    execlp("ffplay", "ffplay",
           "-nodisp",
           "-autoexit",
           "-loglevel", "quiet",
           AUDIO_DIR"/audio.mp3",
           NULL);
    fatal_error("Failed to execute ffplay for audio playback");
}
void draw_ascii_frame(const char *frame_path) {
    if (frame_path == NULL) {
        fatal_error("Invalid frame path provided, path is NULL");
    }

    FILE *file = fopen(frame_path, "r");
    if (file == NULL) {
        fatal_error("Failed to open file: %s", frame_path);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file) != NULL) {
        printf("%s", line);
    }

    fclose(file);
    printf("\n");
}

void draw_frames() {
    // Existing directory checks...

    // Get sorted list of frame files
    char command[1024];
    snprintf(command, sizeof(command), "ls %s/*.txt | sort -V", ASCII_DIR);

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        fatal_error("Failed to list files in directory: %s", ASCII_DIR);
    }

    char file_path[PATH_MAX];
    int frame_count = 0;

    // Clear screen before starting
    printf("\033[2J\033[1;1H");

    // Process each frame in order
    while (fgets(file_path, sizeof(file_path), pipe) != NULL) {
        // Remove newline
        file_path[strcspn(file_path, "\n")] = 0;

        // Clear screen before each frame
        printf("\033[2J\033[1;1H");

        // Draw the frame
        draw_ascii_frame(file_path);
        frame_count++;

        // Add a delay between frames
        int delay_us = 1000000 / atoi(FPS);  // Convert FPS to microseconds
        usleep(delay_us);
    }

    pclose(pipe);
}

void play(){
    if(is_directory_empty(AUDIO_DIR)){
        fatal_error("Audio directory is empty, no audio to play");
    }
    if(is_directory_empty(ASCII_DIR)){
        fatal_error("ASCII directory is empty, no ascii to play");
    }

    pid_t pid = fork();
    if (pid == -1) {
        fatal_error("Fork failed: %s", strerror(errno));
    }
    if(pid == 0){
        play_audio();
    }
    else{

        pid_t pid2 = fork();
        if (pid2 == -1) {
            fatal_error("Fork failed: %s", strerror(errno));
        }
        if(pid2 == 0){
            draw_frames();
        }
        else{
            int status;
            waitpid(pid, &status, 0);
            waitpid(pid2, &status, 0);
        }

        printf("ASCII art playback completed.\n");

    }

}

void empty_directory(const char *dir_name) {

   if(dir_name == NULL) {
      fatal_error("Invalid directory name provided, path is NULL");
   }
    DIR *dir = opendir(dir_name);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip "." and ".." directories
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dir_name, entry->d_name);

            struct stat st;
            if (stat(path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    // Recursively remove subdirectories
                    empty_directory(path);
                    rmdir(path);
                } else {
                    // Remove files
                    unlink(path);
                }
            }
        }
        closedir(dir);
    }
}

 void create_dir(const char *dir_name) {

      if(dir_name == NULL) {
         fatal_error("Invalid directory name provided, path is NULL");
      }
    struct stat st = {0};
    if (stat(dir_name, &st) == 0) {
        // Directory exists, just empty it
        empty_directory(dir_name);
    } else {
        // Create new directory
        if (mkdir(dir_name, 0755) == -1) {
            fatal_error("Failed to create directory: %s (Error: %s)",
                      dir_name, strerror(errno));
        }
    }

    
}

char* get_usage_msg(const char *program_name)
{
    // Allocate memory for the usage message
    char *usage = malloc(BUFFER_SIZE);  // Allocate enough space for the message
    if (usage == NULL) {
        fatal_error("Memory allocation failed for usage message");
    }

    // Format the string with program_name
    snprintf(usage, BUFFER_SIZE, "Usage: %s <video_path>\n", program_name);

    return usage;
}

void extract_audio() {
    pid_t pid = fork();
    if (pid < 0)  fatal_error("fork() failed: %s", strerror(errno));

    if (pid == 0) {
        // child: do work
        execlp("ffmpeg", "ffmpeg",
               "-loglevel", "quiet",
               "-i", VIDEO_PATH, // Input video file
               "-vn",                   // No video
               "-acodec", "libmp3lame", // MP3 codec
               "-q:a", "2",             // Good quality
               AUDIO_DIR"/audio.mp3",
               NULL);
        _exit(EXIT_FAILURE);
    } else {
        // parent: spinner only
        spinner_t *sp = spinner_create("Extracting audio");
        spinner_start(sp);
        waitpid(pid, NULL, 0);
        spinner_stop(sp, true);
        spinner_destroy(sp);
    }
}

void extract_images_grayscale() {
    pid_t pid = fork();
    if (pid < 0)  fatal_error("fork() failed: %s", strerror(errno));

    if (pid == 0) {
        // child: do work
        char output_pattern[512];
        snprintf(output_pattern, sizeof(output_pattern), "%s/gray_%%04d.png", FRAMES_DIR);

        char filter_str[256];
        snprintf(filter_str, sizeof(filter_str), "fps=%s,scale=%s:-1,format=gray",
                 FPS, WIDTH);

        execlp("ffmpeg", "ffmpeg",
               "-loglevel", "quiet",
               "-i", VIDEO_PATH,
               "-vf", filter_str,
               "-start_number", START_FRAME,
               output_pattern,
               NULL);
        _exit(EXIT_FAILURE);
    } else {
        spinner_t *sp = spinner_create("Converting to grayscale");
        spinner_start(sp);
        waitpid(pid, NULL, 0);
        spinner_stop(sp, true);
        spinner_destroy(sp);
    }
}

void batch_convert_to_ascii() {
    pid_t pid = fork();
    if (pid < 0)  fatal_error("fork() failed: %s", strerror(errno));

    if (pid == 0) {
        // child: do work
        DIR *dir = opendir(FRAMES_DIR);
        if (dir == NULL) {
            fatal_error("Failed to open directory: %s", FRAMES_DIR);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char *ext = strrchr(entry->d_name, '.');
            if (ext == NULL || strncmp(ext, ".png", 4) != 0) {
                continue;
            }

            char input_path[PATH_MAX];
            char output_path[PATH_MAX + sizeof(ASCII_DIR) + sizeof(".txt")];

            char base_name[PATH_MAX];
            strncpy(base_name, entry->d_name, ext - entry->d_name);
            base_name[ext - entry->d_name] = '\0';

            snprintf(input_path, sizeof(input_path), "%s/%s", FRAMES_DIR, entry->d_name);
            snprintf(output_path, sizeof(output_path), "%s/%s.txt", ASCII_DIR, base_name);

            pid_t child_pid = fork();
            if (child_pid == 0) {
                char output_arg[sizeof(output_path) + sizeof("--output=")];
                snprintf(output_arg, sizeof(output_arg), "--output=%s", output_path);

                execlp("jp2a", "jp2a", output_arg, input_path, NULL);
                _exit(EXIT_FAILURE);
            } else {
                int status;
                waitpid(child_pid, &status, 0);
            }
        }

        closedir(dir);
        _exit(EXIT_SUCCESS);
    } else {
        spinner_t *sp = spinner_create("Rendering ASCII art");
        spinner_start(sp);
        waitpid(pid, NULL, 0);
        spinner_stop(sp, true);
        spinner_destroy(sp);
    }
}

/**
 * Checks if a directory is empty
 * @param dir_path Path to the directory
 * @return 1 if empty, 0 if not empty, -1 on error
 */
int is_directory_empty(const char *dir_path) {
    if (dir_path == NULL) {
        return -1;
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return -1;  // Error opening directory
    }

    struct dirent *entry;
    int is_empty = 1;  // Assume empty until proven otherwise

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            is_empty = 0;  // Found a file or subdirectory
            break;
        }
    }

    closedir(dir);
    return is_empty;
}

int directory_exists(const char *path) {
    struct stat st;

    // stat() returns 0 on success, -1 on error
    if (stat(path, &st) == 0) {
        // Also verify it's a directory and not a regular file
        if (S_ISDIR(st.st_mode)) {
            return 1;  // Directory exists
        }
    }

    return 0;  // Directory does not exist or is not accessible
}
