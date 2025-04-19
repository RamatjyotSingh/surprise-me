#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   600
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
#include <signal.h>
#include "err.h"
#include "spinner.h"
#include <time.h>    
#include <fnmatch.h> 
#include <ctype.h>       

#define ASSETS_DIR "assets"
#define AUDIO_DIR "assets/audio"
#define ASCII_DIR "assets/ascii"
#define FRAMES_DIR "assets/frames"


#define DEFAULT_FPS  "10"
#define DEFAULT_WIDTH  "900"    // Doubled from 80
#define DEFAULT_HEIGHT "60"     // Doubled from 40
#define DEFAULT_START_TIME "00:00:00" // Start time in HH:MM:SS format
#define DEFAULT_VIDEO_PATH "rr.mp4"
#define DEFAULT_VIDEO_NAME "rr"
#define DEFAULT_DURATION  "0"    // “0” means “no end limit”

#define BUFFER_SIZE 1024

char VIDEO_PATH[PATH_MAX] = DEFAULT_VIDEO_PATH;
char* FPS = DEFAULT_FPS;
char* WIDTH = DEFAULT_WIDTH;
char* HEIGHT = DEFAULT_HEIGHT;
char* START_TIME = DEFAULT_START_TIME;
char* DURATION  = DEFAULT_DURATION;
char VIDEO_NAME[PATH_MAX] = DEFAULT_VIDEO_NAME;


static volatile sig_atomic_t sigint_received = 0;

// SIGINT handler: just record that it happened
static void handle_sigint(int sig) {
    (void)sig;
    sigint_received = 1;
}

void extract_images_grayscale();
char* get_usage_msg(const char *program_name);
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
int is_valid_integer(const char* str);
int is_valid_timestamp(const char* str);

void set_defaults(){
    FPS = DEFAULT_FPS;
    WIDTH = DEFAULT_WIDTH;
    HEIGHT = DEFAULT_HEIGHT;
    START_TIME = DEFAULT_START_TIME;
    DURATION  = DEFAULT_DURATION;
    VIDEO_PATH[0] = '\0';
    VIDEO_NAME[0] = '\0';

}
int main(int argc, char *argv[])
{
    // install our SIGINT handler
    struct sigaction sa = { 0 };
    sa.sa_handler = handle_sigint;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int c, optidx = 0;
    int opts_given = 0;    // ← counter for any options
    int new_file = 0;    // ← counter for any new files

    static struct option longopts[] = {
        { "input",  required_argument, 0, 'i' },
        { "fps",    required_argument, 0, 'f' },
        { "width",  required_argument, 0, 'w' },
        { "height", required_argument, 0, 't' },
        { "start",  required_argument, 0, 's' },
        { "duration", required_argument, 0, 'd' },  // Add duration option
        { "play",   required_argument, 0, 'p' },
        { "reset",  no_argument,       0, 'r' },
        { "help",   no_argument,       0, 'h' },
        { 0,0,0,0 }
    };

    /* only i,f,w,t,s,e,d,p take arguments; r and h are flags */
    while ((c = getopt_long(argc, argv, "i:f:w:t:s:d:p:rh", longopts, &optidx)) != -1) {
        switch (c) {
          case 'i':
            opts_given++;
            new_file = 1;
            strncpy(VIDEO_PATH, optarg, sizeof(VIDEO_PATH)-1);
            break;
          case 'f':  // fps
            opts_given++;
            if (!is_valid_integer(optarg)) {
                user_fatal("Invalid fps value. Must be a positive integer.");
            }
            int fps_value = atoi(optarg);
            if (fps_value <= 0 || fps_value > 60) {
                user_fatal("FPS must be between 1 and 60.");
            }
            FPS = optarg;
            break;

          case 'w':  // width
            opts_given++;
            if (!is_valid_integer(optarg)) {
                user_fatal("Invalid width value. Must be a positive integer.");
            }
            int width_value = atoi(optarg);
            if (width_value <= 0) {
                user_fatal("Width must be positive.");
            }
            WIDTH = optarg;
            break;

          case 't':  // height
            opts_given++;
            if (!is_valid_integer(optarg)) {
                user_fatal("Invalid height value. Must be a positive integer.");
            }
            HEIGHT = optarg;
            break;

          case 's':  // start time
            opts_given++;
            if (!is_valid_timestamp(optarg)) {
                user_fatal("Invalid start time. Format must be HH:MM:SS");
            }
            START_TIME = optarg;
            break;

          case 'd':  // duration
            opts_given++;
            if (!is_valid_integer(optarg)) {
                user_fatal("Invalid duration. Must be a positive integer in seconds.");
            }
            DURATION = optarg;
            break;

          case 'r':
            opts_given++;
            user_warning("This will delete all extracted files and reset settings.");

            reset();
            break;
        case 'p':

            strncpy(VIDEO_NAME, optarg, sizeof(VIDEO_NAME));
            VIDEO_NAME[sizeof(VIDEO_NAME)-1] = '\0';
            
            break;
          case 'h':
            user_error("%s",get_usage_msg(argv[0]));
            exit(EXIT_SUCCESS);
          default:
            user_error("Invalid option provided.");
            exit(EXIT_FAILURE);
        }
    }

   

    if (opts_given) {

        if(new_file){
            char *base = strrchr(VIDEO_PATH, '/');
            base = base ? base+1 : VIDEO_PATH;
            strncpy(VIDEO_NAME, base, sizeof(VIDEO_NAME)-1);
            VIDEO_NAME[sizeof(VIDEO_NAME)-1] = '\0';
            char *dot = strrchr(VIDEO_NAME, '.');
            if (dot) *dot = '\0';
        }

        setup();
    }

    play();
    return EXIT_SUCCESS;
}

void reset()
{
    char response;
    int valid_response = 0;

    // Loop until we get a valid y/n response
    while (!valid_response)
    {
        user_prompt("Are you sure? (y/n)");

        // Read a character and flush the input buffer
        response = getchar();
       

        // Check if response is valid
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

        // Reset the directories
        empty_directory(AUDIO_DIR);
        empty_directory(ASCII_DIR);
        empty_directory(FRAMES_DIR);

        // Set defaults
        set_defaults();

        user_success("Reset completed successfully!");
        exit(EXIT_SUCCESS);
    }
    else
    {
        user_response("Operation cancelled.");
        exit(EXIT_SUCCESS);
    }
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

void  add_new_anim(const char* vid_path){
    if(vid_path == NULL) {
        fatal_error("Invalid path provided for video extraction, path is NULL");
    }
    strncpy(VIDEO_PATH, vid_path, sizeof(VIDEO_PATH)-1);

    extract_audio();
    extract_images_grayscale();
    batch_convert_to_ascii();
}

void play_audio(){
    // redirect ffplay output
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    // play assets/audio/<video_name>.mp3
    char audio_file[PATH_MAX + sizeof(AUDIO_DIR) + sizeof(".mp3") + sizeof(VIDEO_NAME)];
    snprintf(audio_file, sizeof(audio_file), AUDIO_DIR"/%s.mp3", VIDEO_NAME);
    execlp("ffplay", "ffplay",
           "-nodisp",
           "-autoexit",
           "-loglevel", "quiet",
           audio_file,
           NULL);
    fatal_error("Failed to exec ffplay");
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
    char command[PATH_MAX + sizeof(ASCII_DIR) + sizeof("/*.txt") + sizeof(VIDEO_NAME)];
    snprintf(command, sizeof(command), "ls %s/%s*.txt | sort -V", ASCII_DIR, VIDEO_NAME);
  
    
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
        struct timespec ts = {
            .tv_sec  = delay_us / 1000000,
            .tv_nsec = (delay_us % 1000000) * 1000
        };
        nanosleep(&ts, NULL);
    }

    pclose(pipe);
}

int video_extracted(){

    
     char audio_file[PATH_MAX + sizeof(AUDIO_DIR) + sizeof(".mp3") + sizeof(VIDEO_NAME)] = {0} ;
     char ascii_file[PATH_MAX + sizeof(ASCII_DIR) + sizeof(".txt") + sizeof(VIDEO_NAME)] = {0} ;
    snprintf(audio_file, sizeof(audio_file), "%s.mp3", VIDEO_NAME);
    snprintf(ascii_file, sizeof(ascii_file), "%s_gray_*.txt", VIDEO_NAME);



    if (is_directory_empty(AUDIO_DIR) || is_directory_empty(ASCII_DIR) || !dir_contains(AUDIO_DIR, audio_file) || !dir_contains(ASCII_DIR, ascii_file)) {
        return false;
    }
    return true;
}
int dir_contains(const char *dir_path, const char *file_name) {
    if (dir_path == NULL || file_name == NULL) {
        return false;
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(file_name, entry->d_name, 0) == 0) {
            closedir(dir);
            return true;
        }
    }
    
    closedir(dir);
    return false;
}
void play(){
   
    if(!video_extracted() && strcmp(VIDEO_NAME, DEFAULT_VIDEO_NAME) != 0){
        user_fatal("%s  doesn't exists , try inserting a new one with -i <video_path>", VIDEO_NAME);
    }


    pid_t pid = fork();
    if (pid == -1) {
        fatal_error("Fork failed: %s", strerror(errno));
    }
    if(pid == 0){
        draw_frames();
    }
    else{

        pid_t pid2 = fork();
        if (pid2 == -1) {
            fatal_error("Fork failed: %s", strerror(errno));
        }
        if(pid2 == 0){
            play_audio();

        }
        else{
            int status;
            waitpid(pid, &status, 0);
            waitpid(pid2, &status, 0);
        }


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
    if (stat(dir_name, &st) != 0) {
        
   
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

void extract_audio() {

    pid_t pid = fork();
    if (pid == 0) {
        // child: write to assets/audio/<video_name>.mp3
        char out[PATH_MAX + sizeof(AUDIO_DIR) + sizeof("/.mp3") + sizeof(VIDEO_NAME)];
        snprintf(out, sizeof(out), AUDIO_DIR"/%s.mp3", VIDEO_NAME);
        if(access(out, F_OK) == 0) {
            unlink(out);  // Remove the existing file
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
        
        // Add duration parameter if specified
        if (atoi(DURATION) > 0) {
            args[arg_count++] = "-t";
            args[arg_count++] = DURATION;
        }
        
        args[arg_count++] = "-vn";
        args[arg_count++] = "-acodec";
        args[arg_count++] = "libmp3lame";
        args[arg_count++] = "-q:a";
        args[arg_count++] = "2";
        args[arg_count++] = out;
        args[arg_count++] = NULL;  // Terminate the arguments list
        
        execvp("ffmpeg", args);
        _exit(EXIT_FAILURE);
    }
    else if (pid < 0) {
        fatal_error("fork() failed: %s", strerror(errno));
    }
    else {
        /* parent – wait for ffmpeg to finish */
        spinner_t *sp = spinner_create("Extracting audio");
        spinner_start(sp);

        int status;
        waitpid(pid, &status, 0);

        spinner_stop(sp, WIFEXITED(status) && WEXITSTATUS(status) == 0);
        spinner_destroy(sp);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            if (!dir_contains(".",VIDEO_PATH)) {
                user_fatal("Video file not found: %s", VIDEO_PATH);
            } else {
                fatal_error("Failed to extract audio");
            }
        }
    }
}

void extract_images_grayscale() {
    pid_t pid = fork();
    if (pid == 0) {
        char output_pattern[PATH_MAX + sizeof(FRAMES_DIR) + sizeof("_gray_%%04d.png")];
        snprintf(output_pattern, sizeof(output_pattern),
                 "%s/%s_gray_%%04d.png", FRAMES_DIR, VIDEO_NAME);
        
        

        // Build the ffmpeg command with proper time-based extraction
        char *args[20];
        int arg_count = 0;
        
        args[arg_count++] = "ffmpeg";
        args[arg_count++] = "-loglevel";
        args[arg_count++] = "quiet";
        args[arg_count++] = "-ss";
        args[arg_count++] = START_TIME;
        args[arg_count++] = "-i";
        args[arg_count++] = VIDEO_PATH;
        
        // Add duration parameter if specified
        if (atoi(DURATION) > 0) {
            args[arg_count++] = "-t";
            args[arg_count++] = DURATION;
        }
        
        // Apply video filters
        args[arg_count++] = "-vf";
        char vf[BUFFER_SIZE];
        snprintf(vf, sizeof(vf), "fps=%s,scale=%s:-1,format=gray", FPS, WIDTH);
        args[arg_count++] = vf;
        
        args[arg_count++] = output_pattern;
        args[arg_count++] = NULL;  // Terminate the arguments list
        
        execvp("ffmpeg", args);
        _exit(EXIT_FAILURE);
    }
    else if (pid < 0) {
        fatal_error("fork() failed: %s", strerror(errno));
    }
    else {
        /* parent – wait for ffmpeg to finish */
        spinner_t *sp = spinner_create("Extracting frames");
        spinner_start(sp);

        int status;
        waitpid(pid, &status, 0);

        spinner_stop(sp, WIFEXITED(status) && WEXITSTATUS(status) == 0);
        spinner_destroy(sp);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            // Check if the error is because the video file doesn't exist
            if (!dir_contains(".",VIDEO_PATH)) {
                user_fatal("Video file not found: %s", VIDEO_PATH);
            } else {
                fatal_error("Failed to extract frames");
            }
        }
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

// Validate that a string contains only digits
int is_valid_integer(const char* str) {
    if (str == NULL || *str == '\0') return 0;
    
    // Check each character is a digit
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') return 0;
    }
    return 1;
}

// Validate that a string is in HH:MM:SS format
int is_valid_timestamp(const char* str) {
    if (str == NULL) return 0;
    
    // Check length and format (HH:MM:SS)
    if (strlen(str) != 8) return 0;
    if (str[2] != ':' || str[5] != ':') return 0;
    
    // Check hours (00-23)
    if (!isdigit(str[0]) || !isdigit(str[1])) return 0;
    int hours = (str[0] - '0') * 10 + (str[1] - '0');
    if (hours > 23) return 0;
    
    // Check minutes (00-59)
    if (!isdigit(str[3]) || !isdigit(str[4])) return 0;
    int minutes = (str[3] - '0') * 10 + (str[4] - '0');
    if (minutes > 59) return 0;
    
    // Check seconds (00-59)
    if (!isdigit(str[6]) || !isdigit(str[7])) return 0;
    int seconds = (str[6] - '0') * 10 + (str[7] - '0');
    if (seconds > 59) return 0;
    
    return 1;
}
