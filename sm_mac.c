/*******************************************************************************
 * ASCII Video Player - macOS Version
 *
 * This program converts video files to ASCII art and plays them in the terminal
 * with synchronized audio.
 ******************************************************************************/

/* Standard library includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include "err.h"
#include "spinner.h"
#include <time.h>
#include <fnmatch.h>
#include <ctype.h>

/* Path definitions for macOS - conditionally define PATH_MAX if not already defined */
#ifndef PATH_MAX
#define PATH_MAX 1024 /* macOS has a different PATH_MAX value */
#endif

/* Buffer sizes */
#define BUFFER_SIZE 1024
#define PATH_BUFFER_SIZE 2048 /* Larger buffer for path operations */

/* Directory structure for assets */
#define ASSETS_DIR "assets"
#define AUDIO_DIR "assets/audio"
#define ASCII_DIR "assets/ascii"
#define FRAMES_DIR "assets/frames"

/* Default configuration values */
#define DEFAULT_FPS "10"
#define DEFAULT_WIDTH "900"
#define DEFAULT_HEIGHT "600"
#define DEFAULT_START_TIME "00:00:00"
#define DEFAULT_VIDEO_PATH "rr.mp4"
#define DEFAULT_VIDEO_NAME "rr"
#define DEFAULT_DURATION "0"

/* Global variables for configuration */
char VIDEO_PATH[PATH_MAX] = DEFAULT_VIDEO_PATH;
char *FPS = DEFAULT_FPS;
char *WIDTH = DEFAULT_WIDTH;
char *HEIGHT = DEFAULT_HEIGHT;
char *START_TIME = DEFAULT_START_TIME;
char *DURATION = DEFAULT_DURATION;
char VIDEO_NAME[PATH_MAX] = DEFAULT_VIDEO_NAME;

/* Flag for signal handling */
static volatile sig_atomic_t sigint_received = 0;

/* Forward declarations of functions */
void extract_images_grayscale();
char *get_usage_msg(const char *program_name);
void create_dir(const char *dir_name);
void empty_directory(const char *dir_name);
void setup();
void reset();
void play();
void draw_frames();
void draw_ascii_frame(const char *frame_path);
void batch_convert_to_ascii();
void extract_audio();
void play_audio();
int directory_exists(const char *path);
int is_directory_empty(const char *dir_path);
int dir_contains(const char *dir_path, const char *file_name);
int video_extracted();
int is_valid_integer(const char *str);
int is_valid_timestamp(const char *str);

/**
 * SIGINT signal handler
 */
static void handle_sigint(int sig)
{
    (void)sig;
    sigint_received = 1;
}

/**
 * Reset all configuration values to defaults
 */
void set_defaults()
{
    FPS = DEFAULT_FPS;
    WIDTH = DEFAULT_WIDTH;
    HEIGHT = DEFAULT_HEIGHT;
    START_TIME = DEFAULT_START_TIME;
    DURATION = DEFAULT_DURATION;
    VIDEO_PATH[0] = '\0';
    VIDEO_NAME[0] = '\0';
}

/**
 * Main program entry point
 */
int main(int argc, char *argv[])
{
    /* Install SIGINT handler */
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int c, optidx, opts_given = 0;

    /* Define long options */
    static struct option longopts[] = {
        {"input", required_argument, 0, 'i'},
        {"fps", required_argument, 0, 'f'},
        {"width", required_argument, 0, 'w'},
        {"height", required_argument, 0, 't'},
        {"start", required_argument, 0, 's'},
        {"duration", required_argument, 0, 'd'},
        {"play", required_argument, 0, 'p'},
        {"reset", no_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    /* Parse command line options */
    while ((c = getopt_long(argc, argv, "i:f:w:t:s:d:p:rh", longopts, &optidx)) != -1)
    {
        switch (c)
        {
        case 'i': /* Input video file */
            strncpy(VIDEO_PATH, optarg, sizeof(VIDEO_PATH) - 1);
            VIDEO_PATH[sizeof(VIDEO_PATH) - 1] = '\0';
            opts_given++;
            break;
        case 'f': /* Frames per second */
            if (!is_valid_integer(optarg))
            {
                user_fatal("Invalid fps value. Must be a positive integer.");
            }
            int fps_value = atoi(optarg);
            if (fps_value <= 0 || fps_value > 60)
            {
                user_fatal("FPS must be between 1 and 60.");
            }
            FPS = optarg;
            opts_given++;
            break;
        case 'w': /* Width in characters */
            if (!is_valid_integer(optarg))
            {
                user_fatal("Invalid width value. Must be a positive integer.");
            }
            int width_value = atoi(optarg);
            if (width_value <= 0)
            {
                user_fatal("Width must be positive.");
            }
            WIDTH = optarg;
            opts_given++;
            break;
        case 't': /* Height in characters */
            if (!is_valid_integer(optarg))
            {
                user_fatal("Invalid height value. Must be a positive integer.");
            }
            HEIGHT = optarg;
            opts_given++;
            break;
        case 's': /* Start time */
            if (!is_valid_timestamp(optarg))
            {
                user_fatal("Invalid start time. Format must be HH:MM:SS");
            }
            START_TIME = optarg;
            opts_given++;
            break;
        case 'd': /* Duration */
            if (!is_valid_integer(optarg))
            {
                user_fatal("Invalid duration. Must be a positive integer in seconds.");
            }
            DURATION = optarg;
            opts_given++;
            break;
        case 'r': /* Reset settings and clear extracted files */
            user_warning("This will delete all extracted files and reset settings.");
            reset();
            exit(EXIT_SUCCESS);
            break;
        case 'p': /* Play a previously extracted video */
            strncpy(VIDEO_NAME, optarg, sizeof(VIDEO_NAME) - 1);
            VIDEO_NAME[sizeof(VIDEO_NAME) - 1] = '\0';
            play();
            exit(EXIT_SUCCESS);
            break;
        case 'h': /* Display help message */
            user_error("%s", get_usage_msg(argv[0]));
            exit(EXIT_SUCCESS);
        default: /* Invalid option */
            user_error("Invalid option provided.");
            user_error("%s", get_usage_msg(argv[0]));
            exit(EXIT_FAILURE);
        }
    }

    /* If any option was given, extract the video name from path and process the video */
    if (opts_given != 0)
    {
        /* Extract the base filename without path or extension */
        char *base = strrchr(VIDEO_PATH, '/');
        base = base ? base + 1 : VIDEO_PATH;
        strncpy(VIDEO_NAME, base, sizeof(VIDEO_NAME) - 1);
        VIDEO_NAME[sizeof(VIDEO_NAME) - 1] = '\0';

        /* Remove extension from filename */
        char *dot = strrchr(VIDEO_NAME, '.');
        if (dot)
            *dot = '\0';

        /* Process the video */
        setup();
    }

    /* Play the video */
    play();
    return EXIT_SUCCESS;
}

/**
 * Reset all directories and return configuration to defaults
 */
void reset()
{
    char response;
    int valid_response = 0;

    // Loop until a valid y/n response is received
    while (!valid_response)
    {
        user_prompt("Are you sure? (y/n)");
        response = getchar();
        
        // Clear the input buffer - important for macOS terminal
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
        
        if (response == 'y' || response == 'Y' || response == 'n' || response == 'N')
        {
            valid_response = 1;
        }
        else
        {
            user_error("Please enter 'y' or 'n'");
        }
    }

    if (response == 'y' || response == 'Y')
    {
        user_info("Resetting all directories...");
        empty_directory(AUDIO_DIR);
        empty_directory(ASCII_DIR);
        empty_directory(FRAMES_DIR);
        set_defaults();
        user_success("Reset completed successfully!");
    }
    else
    {
        user_response("Operation cancelled.");
    }
}

/**
 * Set up the environment for video processing
 */
void setup()
{
    // Validate video path exists
    if (VIDEO_PATH[0] == '\0')
    {
        fatal_error("Invalid path provided for video extraction, path is NULL");
    }

    // Create the necessary directory structure if it doesn't exist
    create_dir(ASSETS_DIR);
    create_dir(ASCII_DIR);
    create_dir(AUDIO_DIR);
    create_dir(FRAMES_DIR);

    // Extract components from the video file
    extract_audio();
    extract_images_grayscale();
    batch_convert_to_ascii();
}

/**
 * Find an available media player for macOS
 */
char* find_available_player()
{
    static const char* players[] = {
        "ffplay", "mpv", "mplayer", "vlc", "afplay", NULL
    };
    
    // Check in common macOS binary locations
    const char* paths[] = {
        "/usr/bin/%s",
        "/usr/local/bin/%s",
        "/opt/homebrew/bin/%s",
        "/opt/local/bin/%s",
        NULL
    };
    
    for (int i = 0; players[i] != NULL; i++) {
        for (int j = 0; paths[j] != NULL; j++) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), paths[j], players[i]);
            if (access(path, X_OK) == 0) {
                return (char*)players[i];
            }
        }
    }
    
    return NULL;
}

/**
 * Play the audio track of the current video
 */
void play_audio()
{
    // Redirect output
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    // Build audio file path
    char audio_file[8096];
    snprintf(audio_file, sizeof(audio_file), "%s/%s.mp3", AUDIO_DIR, VIDEO_NAME);

    // Find available player
    char* player = find_available_player();
    if (!player) {
        user_fatal("No media player found. Please install ffplay, mpv, or afplay.");
    }
    
    // Execute appropriate player with right arguments
    if (strcmp(player, "ffplay") == 0) {
        execlp(player, player, "-nodisp", "-autoexit", "-loglevel", "quiet", audio_file, NULL);
    } else if (strcmp(player, "mpv") == 0) {
        execlp(player, player, "--no-video", "--really-quiet", audio_file, NULL);
    } else if (strcmp(player, "mplayer") == 0) {
        execlp(player, player, "-novideo", "-really-quiet", audio_file, NULL);
    } else if (strcmp(player, "vlc") == 0) {
        execlp(player, player, "--intf", "dummy", "--no-video", audio_file, NULL);
    } else if (strcmp(player, "afplay") == 0) {
        // macOS built-in audio player
        execlp(player, player, audio_file, NULL);
    }
    
    // If we get here, execution failed
    fatal_error("Failed to play audio with %s", player);
}

/**
 * Render a single ASCII art frame to the terminal
 */
void draw_ascii_frame(const char *frame_path)
{
    if (frame_path == NULL)
    {
        fatal_error("Invalid frame path provided, path is NULL");
    }

    FILE *file = fopen(frame_path, "r");
    if (file == NULL)
    {
        fatal_error("Failed to open file: %s", frame_path);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        printf("%s", line);
    }

    fclose(file);
    printf("\n");
}

/**
 * Draw ASCII frames in sequence to create video playback
 */
void draw_frames()
{
    // Use a different approach to get sorted frames on macOS
    DIR *dir = opendir(ASCII_DIR);
    if (!dir) {
        fatal_error("Failed to open directory: %s", ASCII_DIR);
    }
    
    // Read all frame files matching our pattern
    struct dirent *entry;
    char **files = NULL;
    int file_count = 0;
    int capacity = 0;
    
    char pattern[8096];
    snprintf(pattern, sizeof(pattern), "%s_gray_*.txt", VIDEO_NAME);
    
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(pattern, entry->d_name, 0) == 0) {
            // Expand array if needed
            if (file_count >= capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                char **new_files = realloc(files, capacity * sizeof(char *));
                if (!new_files) {
                    fatal_error("Memory allocation failed");
                }
                files = new_files;
            }
            
            // Store the file path
            char *path = malloc(8096);
            if (!path) {
                fatal_error("Memory allocation failed");
            }
            snprintf(path, 8096, "%s/%s", ASCII_DIR, entry->d_name);
            files[file_count++] = path;
        }
    }
    closedir(dir);
    
    // Sort files (simple numeric sort based on frame numbers)
    for (int i = 0; i < file_count - 1; i++) {
        for (int j = 0; j < file_count - i - 1; j++) {
            if (strcmp(files[j], files[j + 1]) > 0) {
                char *temp = files[j];
                files[j] = files[j + 1];
                files[j + 1] = temp;
            }
        }
    }
    
    // Clear screen before starting playback
    printf("\033[2J\033[1;1H");
    
    // Process each frame
    for (int i = 0; i < file_count && !sigint_received; i++) {
        // Clear screen before each frame
        printf("\033[2J\033[1;1H");
        
        // Draw the current frame
        draw_ascii_frame(files[i]);
        
        // Calculate delay based on FPS
        int delay_us = 1000000 / atoi(FPS);
        
        // macOS doesn't have nanosleep, use usleep instead
        usleep(delay_us);
        
        // Free the memory for this filename
        free(files[i]);
    }
    
    // Free the array
    free(files);
}

/**
 * Play the ASCII video with synchronized audio
 */
void play()
{
    // Verify video assets exist
    if (is_directory_empty(ASCII_DIR))
    {
        user_fatal("No ASCII art frames found. Please extract video using -i <video_path> first.");
    }
    if (!video_extracted() && strcmp(VIDEO_NAME, DEFAULT_VIDEO_NAME) != 0)
    {
        user_fatal("%s doesn't exist, try inserting a new one with -i <video_path>", VIDEO_NAME);
    }

    // Create first child process for displaying ASCII frames
    pid_t pid = fork();
    if (pid == -1)
    {
        fatal_error("Fork failed: %s", strerror(errno));
    }
    if (pid == 0)
    {
        // Child process: display the ASCII frames
        draw_frames();
        exit(EXIT_SUCCESS);
    }
    else
    {
        // Create second child process for playing audio
        pid_t pid2 = fork();
        if (pid2 == -1)
        {
            fatal_error("Fork failed: %s", strerror(errno));
        }
        if (pid2 == 0)
        {
            // Second child process: play the audio track
            play_audio();
            exit(EXIT_SUCCESS);
        }
        else
        {
            // Parent process: wait for both child processes
            int status;
            waitpid(pid, &status, 0);
            waitpid(pid2, &status, 0);
        }
    }
}

/**
 * Empty a directory by removing all its files and subdirectories
 */
void empty_directory(const char *dir_name)
{
    if (dir_name == NULL)
    {
        fatal_error("Invalid directory name provided, path is NULL");
    }

    DIR *dir = opendir(dir_name);
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dir_name, entry->d_name);

            struct stat st;
            if (stat(path, &st) == 0)
            {
                if (S_ISDIR(st.st_mode))
                {
                    empty_directory(path);
                    rmdir(path);
                }
                else
                {
                    unlink(path);
                }
            }
        }
        closedir(dir);
    }
}

/**
 * Create a directory if it doesn't already exist
 */
void create_dir(const char *dir_name)
{
    if (dir_name == NULL)
    {
        fatal_error("Invalid directory name provided, path is NULL");
    }

    struct stat st = {0};
    if (stat(dir_name, &st) != 0)
    {
        if (mkdir(dir_name, 0755) == -1)
        {
            fatal_error("Failed to create directory: %s (Error: %s)",
                      dir_name, strerror(errno));
        }
    }
}

/**
 * Check if a directory contains files matching a specified pattern
 */
int dir_contains(const char *dir_path, const char *file_name)
{
    if (dir_path == NULL || file_name == NULL)
    {
        return 0;
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (fnmatch(file_name, entry->d_name, 0) == 0)
        {
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

/**
 * Checks if a directory is empty
 */
int is_directory_empty(const char *dir_path)
{
    if (dir_path == NULL)
    {
        return -1;
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return -1;
    }

    struct dirent *entry;
    int is_empty = 1;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            is_empty = 0;
            break;
        }
    }

    closedir(dir);
    return is_empty;
}

/**
 * Check if the current video has been properly extracted
 */
int video_extracted()
{
    char audio_file[8096] = {0};
    char ascii_file[8096] = {0};

    snprintf(audio_file, sizeof(audio_file), "%s.mp3", VIDEO_NAME);
    snprintf(ascii_file, sizeof(ascii_file), "%s_gray_*.txt", VIDEO_NAME);

    if (is_directory_empty(AUDIO_DIR) ||
        is_directory_empty(ASCII_DIR) ||
        !dir_contains(AUDIO_DIR, audio_file) ||
        !dir_contains(ASCII_DIR, ascii_file))
    {
        return 0;
    }

    return 1;
}

/**
 * Extract audio track from the input video file
 */
void extract_audio()
{
    pid_t pid = fork();
    if (pid == 0)
    {
        char out[8096];
        snprintf(out, sizeof(out), AUDIO_DIR "/%s.mp3", VIDEO_NAME);

        if (access(out, F_OK) == 0)
        {
            unlink(out);
        }

        char *args[20];
        int arg_count = 0;

        args[arg_count++] = "ffmpeg";
        args[arg_count++] = "-loglevel";
        args[arg_count++] = "quiet";
        args[arg_count++] = "-ss";
        args[arg_count++] = START_TIME;
        args[arg_count++] = "-i";
        args[arg_count++] = VIDEO_PATH;

        if (atoi(DURATION) > 0)
        {
            args[arg_count++] = "-t";
            args[arg_count++] = DURATION;
        }

        args[arg_count++] = "-vn";
        args[arg_count++] = "-acodec";
        args[arg_count++] = "libmp3lame";
        args[arg_count++] = "-q:a";
        args[arg_count++] = "2";
        args[arg_count++] = out;
        args[arg_count++] = NULL;

        execvp("ffmpeg", args);
        _exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        fatal_error("fork() failed: %s", strerror(errno));
    }
    else
    {
        spinner_t *sp = spinner_create("Extracting audio");
        spinner_start(sp);

        int status;
        waitpid(pid, &status, 0);

        spinner_stop(sp, WIFEXITED(status) && WEXITSTATUS(status) == 0);
        spinner_destroy(sp);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            if (!dir_contains(".", VIDEO_PATH))
            {
                user_fatal("Video file not found: %s", VIDEO_PATH);
            }
            else
            {
                fatal_error("Failed to extract audio");
            }
        }
    }
}

/**
 * Extract grayscale frames from the input video file
 */
void extract_images_grayscale()
{
    pid_t pid = fork();
    if (pid == 0)
    {
        char output_pattern[8096];
        snprintf(output_pattern, sizeof(output_pattern),
                 "%s/%s_gray_%%04d.png", FRAMES_DIR, VIDEO_NAME);

        char *args[20];
        int arg_count = 0;

        args[arg_count++] = "ffmpeg";
        args[arg_count++] = "-loglevel";
        args[arg_count++] = "quiet";
        args[arg_count++] = "-ss";
        args[arg_count++] = START_TIME;
        args[arg_count++] = "-i";
        args[arg_count++] = VIDEO_PATH;

        if (atoi(DURATION) > 0)
        {
            args[arg_count++] = "-t";
            args[arg_count++] = DURATION;
        }

        args[arg_count++] = "-vf";
        char vf[BUFFER_SIZE];
        snprintf(vf, sizeof(vf), "fps=%s,scale=%s:-1,format=gray", FPS, WIDTH);
        args[arg_count++] = vf;

        args[arg_count++] = output_pattern;
        args[arg_count++] = NULL;

        execvp("ffmpeg", args);
        _exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        fatal_error("fork() failed: %s", strerror(errno));
    }
    else
    {
        spinner_t *sp = spinner_create("Extracting frames");
        spinner_start(sp);

        int status;
        waitpid(pid, &status, 0);

        spinner_stop(sp, WIFEXITED(status) && WEXITSTATUS(status) == 0);
        spinner_destroy(sp);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            if (!dir_contains(".", VIDEO_PATH))
            {
                user_fatal("Video file not found: %s", VIDEO_PATH);
            }
            else
            {
                fatal_error("Failed to extract frames");
            }
        }
    }
}

/**
 * Convert all extracted video frames to ASCII art
 */
void batch_convert_to_ascii()
{
    pid_t pid = fork();
    if (pid < 0)
        fatal_error("fork() failed: %s", strerror(errno));

    if (pid == 0)
    {
        DIR *dir = opendir(FRAMES_DIR);
        if (dir == NULL)
        {
            fatal_error("Failed to open directory: %s", FRAMES_DIR);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            char *ext = strrchr(entry->d_name, '.');
            if (ext == NULL || strncmp(ext, ".png", 4) != 0)
            {
                continue;
            }

            char input_path[8096];
            char output_path[8096];
            char base_name[PATH_MAX];
            
            size_t name_len = ext - entry->d_name;
            strncpy(base_name, entry->d_name, name_len);
            base_name[name_len] = '\0';

            snprintf(input_path, sizeof(input_path), "%s/%s", FRAMES_DIR, entry->d_name);
            snprintf(output_path, sizeof(output_path), "%s/%s.txt", ASCII_DIR, base_name);

            pid_t child_pid = fork();
            if (child_pid == 0)
            {
                char output_arg[8096 + sizeof(output_path) + sizeof("--output=")];
                snprintf(output_arg, sizeof(output_arg), "--output=%s", output_path);

                execlp("jp2a", "jp2a", output_arg, input_path, NULL);
                _exit(EXIT_FAILURE);
            }
            else
            {
                int status;
                waitpid(child_pid, &status, 0);
            }
        }

        closedir(dir);
        exit(EXIT_SUCCESS);
    }
    else
    {
        spinner_t *sp = spinner_create("Rendering ASCII art");
        spinner_start(sp);

        waitpid(pid, NULL, 0);

        spinner_stop(sp, true);
        spinner_destroy(sp);
    }
}

/**
 * Check if a directory exists at the specified path
 */
int directory_exists(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            return 1;
        }
    }
    return 0;
}

/**
 * Generate a detailed usage message with instructions and examples
 */
char *get_usage_msg(const char *program_name)
{
    char *usage = malloc(BUFFER_SIZE);
    if (usage == NULL)
    {
        fatal_error("Memory allocation failed for usage message");
    }

    snprintf(usage, BUFFER_SIZE,
             "Usage: %s [OPTIONS]\n\n"
             "Options:\n"
             "  -i, --input FILE       Path to a video file to process\n"
             "  -f, --fps N            Frames per second (default: %s)\n"
             "  -w, --width N          Width in characters (default: %s)\n"
             "  -t, --height N         Height in characters (default: %s)\n"
             "  -s, --start TIME       Start time in HH:MM:SS format (default: %s)\n"
             "  -d, --duration SEC     Duration in seconds (default: full video)\n"
             "  -p, --play NAME        Play a previously converted video by name\n"
             "  -r, --reset            Reset all settings and delete all extracted files\n"
             "                         WARNING: This will permanently delete all videos!\n"
             "  -h, --help             Display this help message\n\n"
             "Examples:\n"
             "  %s -p rr               Play the default \"rickroll\" video\n"
             "  %s -i video.mp4        Convert and play a new video\n"
             "  %s -i video.mp4 -s 00:01:30 -d 10  Start at 1:30, play for 10 seconds\n",
             program_name, DEFAULT_FPS, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_START_TIME,
             program_name, program_name, program_name);

    return usage;
}

/**
 * Validate that a string contains only digits
 */
int is_valid_integer(const char *str)
{
    if (str == NULL || *str == '\0')
        return 0;

    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] < '0' || str[i] > '9')
            return 0;
    }

    return 1;
}

/**
 * Validate that a string represents a valid timestamp in HH:MM:SS format
 */
int is_valid_timestamp(const char *str)
{
    if (str == NULL)
        return 0;

    if (strlen(str) != 8)
        return 0;
    if (str[2] != ':' || str[5] != ':')
        return 0;

    if (!isdigit(str[0]) || !isdigit(str[1]))
        return 0;
    int hours = (str[0] - '0') * 10 + (str[1] - '0');
    if (hours > 23)
        return 0;

    if (!isdigit(str[3]) || !isdigit(str[4]))
        return 0;
    int minutes = (str[3] - '0') * 10 + (str[4] - '0');
    if (minutes > 59)
        return 0;

    if (!isdigit(str[6]) || !isdigit(str[7]))
        return 0;
    int seconds = (str[6] - '0') * 10 + (str[7] - '0');
    if (seconds > 59)
        return 0;

    return 1;
}
