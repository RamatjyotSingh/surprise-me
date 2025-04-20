/*******************************************************************************
 * ASCII Video Player
 * 
 * This program converts video files to ASCII art and plays them in the terminal
 * with synchronized audio.
 ******************************************************************************/

/* Enable POSIX and XOPEN features for functions like nanosleep */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 600

/* Standard library includes */
#include <stdio.h>      /* Standard I/O functions */
#include <stdlib.h>     /* Standard library functions like malloc, free */
#include <stdint.h>     /* Integer types with specified widths */
#include <unistd.h>     /* POSIX operating system API */
#include <sys/types.h>  /* Basic system data types */
#include <sys/wait.h>   /* Process control and waiting */
#include <string.h>     /* String handling functions */
#include <sys/stat.h>   /* File status and information */
#include <errno.h>      /* System error numbers */
#include <ftw.h>        /* File tree walk */
#include <linux/limits.h> /* System limits like PATH_MAX */
#include <dirent.h>     /* Directory entry functions */
#include <fcntl.h>      /* File control options */
#include <getopt.h>     /* Command-line option parsing */
#include <signal.h>     /* Signal handling */
#include "err.h"        /* Custom error handling */
#include "spinner.h"    /* Custom loading spinner */
#include <time.h>       /* Time and date functions */
#include <fnmatch.h>    /* Filename matching */
#include <ctype.h>      /* Character type functions */

/* Directory structure for assets */
#define ASSETS_DIR "assets"           /* Main assets directory */
#define AUDIO_DIR "assets/audio"      /* Directory for extracted audio */
#define ASCII_DIR "assets/ascii"      /* Directory for ASCII art frames */
#define FRAMES_DIR "assets/frames"    /* Directory for extracted video frames */

/* Default configuration values */
#define DEFAULT_FPS "10"              /* Frames per second for playback */
#define DEFAULT_WIDTH "900"           /* Width of ASCII output in characters */
#define DEFAULT_HEIGHT "600"          /* Height of ASCII output in characters */
#define DEFAULT_START_TIME "00:00:00" /* Start time in HH:MM:SS format */
#define DEFAULT_VIDEO_PATH "rr.mp4"   /* Default video file path */
#define DEFAULT_VIDEO_NAME "rr"       /* Default video name (without extension) */
#define DEFAULT_DURATION "0"          /* Duration in seconds (0 means full video) */

#define BUFFER_SIZE 1024              /* Standard buffer size for I/O operations */

/* Global variables for configuration */
char VIDEO_PATH[PATH_MAX] = DEFAULT_VIDEO_PATH;  /* Path to the input video file */
char *FPS = DEFAULT_FPS;              /* Frames per second for video playback */
char *WIDTH = DEFAULT_WIDTH;          /* Width of ASCII output */
char *HEIGHT = DEFAULT_HEIGHT;        /* Height of ASCII output */
char *START_TIME = DEFAULT_START_TIME; /* Start time for video extraction */
char *DURATION = DEFAULT_DURATION;    /* Duration to extract (0 = full video) */
char VIDEO_NAME[PATH_MAX] = DEFAULT_VIDEO_NAME; /* Name of the video (without extension) */

/* Flag for signal handling */
static volatile sig_atomic_t sigint_received = 0;

/**
 * SIGINT signal handler
 * Sets a flag when Ctrl+C is pressed but doesn't terminate the program immediately
 * 
 * @param sig Signal number (unused)
 */
static void handle_sigint(int sig)
{
    (void)sig; /* Cast to void to suppress unused parameter warning */
    sigint_received = 1;
}

/* Forward declarations of functions */
void extract_images_grayscale();  /* Extract frames from video as grayscale images */
char *get_usage_msg(const char *program_name); /* Generate usage message */
void create_dir(const char *dir_name); /* Create directory if it doesn't exist */
void empty_directory(const char *dir_name); /* Remove all files in directory */
void setup(); /* Setup directories and extract video/audio */
void reset(); /* Reset directories and settings */
void play(); /* Play the ASCII video with audio */
void draw_frames(); /* Display ASCII frames in sequence */
void draw_ascii_frame(const char *frame_path); /* Display a single ASCII frame */
void batch_convert_to_ascii(); /* Convert grayscale images to ASCII art */
void extract_audio(); /* Extract audio from video */
void play_audio(); /* Play extracted audio */
int directory_exists(const char *path); /* Check if directory exists */
int is_directory_empty(const char *dir_path); /* Check if directory is empty */
int dir_contains(const char *dir_path, const char *file_name); /* Check if directory contains file matching pattern */
int video_extracted(); /* Check if video has been extracted */
int is_valid_integer(const char *str); /* Validate string is a positive integer */
int is_valid_timestamp(const char *str); /* Validate string is in HH:MM:SS format */

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
    VIDEO_PATH[0] = '\0';  /* Clear video path */
    VIDEO_NAME[0] = '\0';  /* Clear video name */
}

/**
 * Main program entry point
 * 
 * Parses command line arguments and executes requested operations
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit status code
 */
int main(int argc, char *argv[])
{
    /* Install SIGINT handler to gracefully handle Ctrl+C */
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sa.sa_flags = SA_RESTART;  /* Automatically restart interrupted system calls */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int c, optidx, opts_given = 0;

    /* Define long options for command line argument parsing */
    static struct option longopts[] = {
        {"input", required_argument, 0, 'i'},    /* Input video file */
        {"fps", required_argument, 0, 'f'},      /* Frames per second */
        {"width", required_argument, 0, 'w'},    /* Width in characters */
        {"height", required_argument, 0, 't'},   /* Height in characters */
        {"start", required_argument, 0, 's'},    /* Start time */
        {"duration", required_argument, 0, 'd'}, /* Duration to extract */
        {"play", required_argument, 0, 'p'},     /* Play a previously extracted video */
        {"reset", no_argument, 0, 'r'},          /* Reset settings and clear extracted files */
        {"help", no_argument, 0, 'h'},           /* Display help */
        {0, 0, 0, 0}                             /* End of options */
    };

    /* Parse command line options */
    while ((c = getopt_long(argc, argv, "i:f:w:t:s:d:p:rh", longopts, &optidx)) != -1)
    {
        switch (c)
        {
        case 'i': /* Input video file */
            strncpy(VIDEO_PATH, optarg, sizeof(VIDEO_PATH) - 1);
            opts_given++;
            break;
        case 'f': /* Frames per second */
            /* Validate FPS is a positive integer between 1-60 */
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
            /* Validate width is a positive integer */
            if (!is_valid_integer(optarg))
            {
                user_fatal("Invalid width value. Must be a positive integer.");
            }
            int width_value = atoi(optarg);
            if (width_value <= 0)
            {
                user_fatal("Width must be positive.");
            }

            opts_given++;
            WIDTH = optarg;
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
            /* Validate start time is in HH:MM:SS format */
            if (!is_valid_timestamp(optarg))
            {
                user_fatal("Invalid start time. Format must be HH:MM:SS");
            }
            START_TIME = optarg;
            opts_given++;
            break;

        case 'd': /* Duration */
            /* Validate duration is a positive integer */
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
            /* Copy the video name and play it */
            strncpy(VIDEO_NAME, optarg, sizeof(VIDEO_NAME));
            VIDEO_NAME[sizeof(VIDEO_NAME) - 1] = '\0'; /* Ensure null termination */
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
        VIDEO_NAME[sizeof(VIDEO_NAME) - 1] = '\0'; /* Ensure null termination */
        
        /* Remove extension from filename */
        char *dot = strrchr(VIDEO_NAME, '.');
        if (dot)
            *dot = '\0';

        /* Process the video */
        setup();
    }
    
    /* Play the video (either the default or the one that was just processed) */
    play();
    return EXIT_SUCCESS;
}
/**
 * Reset all directories and return configuration to defaults
 * 
 * This function prompts the user for confirmation before removing all extracted
 * files and resetting the configuration to default values. This operation 
 * cannot be undone.
 */
void reset()
{
    char response;
    int valid_response = 0;

    // Loop until a valid y/n response is received
    while (!valid_response)
    {
        user_prompt("Are you sure? (y/n)");

        // Read a character and flush the input buffer
        response = getchar();

        // Validate the response is either y, Y, n, or N
        if (response == 'y' || response == 'Y' || response == 'n' || response == 'N')
        {
            valid_response = 1;
        }
        else
        {
            user_error("Please enter 'y' or 'n'");
        }
    }

    // Only proceed with reset if user confirmed with 'y'
    if (response == 'y' || response == 'Y')
    {
        user_info("Resetting all directories...");

        // Clear all content from the asset directories
        empty_directory(AUDIO_DIR);
        empty_directory(ASCII_DIR);
        empty_directory(FRAMES_DIR);

        // Reset all configuration options to default values
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
 * 
 * This function prepares the directory structure and extracts the necessary
 * assets from the video file: audio track, video frames, and converts frames
 * to ASCII art. This is the main preparation step before playback.
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
    extract_audio();          // Extract audio track
    extract_images_grayscale(); // Extract video frames as grayscale images
    batch_convert_to_ascii(); // Convert frames to ASCII art
}

/**
 * Play the audio track of the current video
 * 
 * This function uses ffplay to play the extracted audio file
 * corresponding to the current video. All ffplay output is redirected
 * to /dev/null to avoid cluttering the terminal.
 */
void play_audio()
{
    // Redirect standard output and error to /dev/null to suppress ffplay output
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    // Construct path to the audio file for current video
    char audio_file[PATH_MAX + sizeof(AUDIO_DIR) + sizeof(".mp3") + sizeof(VIDEO_NAME)];
    snprintf(audio_file, sizeof(audio_file), AUDIO_DIR "/%s.mp3", VIDEO_NAME);
    
    // Execute ffplay to play the audio file without display and exit when done
    execlp("ffplay", "ffplay",
           "-nodisp",      // No video display
           "-autoexit",    // Exit when playback is complete
           "-loglevel", "quiet", // Suppress all messages
           audio_file,     // Path to the audio file
           NULL);
           
    // This line only executes if execlp fails
    fatal_error("Failed to exec ffplay");
}

/**
 * Render a single ASCII art frame to the terminal
 * 
 * This function reads an ASCII art file and prints its contents to stdout,
 * displaying a single frame of the ASCII video.
 * 
 * @param frame_path Path to the ASCII art file to display
 */
void draw_ascii_frame(const char *frame_path)
{
    // Validate frame path is not NULL
    if (frame_path == NULL)
    {
        fatal_error("Invalid frame path provided, path is NULL");
    }

    // Open the ASCII frame file
    FILE *file = fopen(frame_path, "r");
    if (file == NULL)
    {
        fatal_error("Failed to open file: %s", frame_path);
    }

    // Read and display each line of the ASCII art
    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        printf("%s", line); // Print each line to stdout
    }

    // Clean up and add a trailing newline
    fclose(file);
    printf("\n");
}
/**
 * Draw ASCII frames in sequence to create video playback
 * 
 * This function creates the visual playback by displaying ASCII art frames
 * in the terminal at the specified frame rate. It:
 * 1. Gets a sorted list of frame files for the current video
 * 2. Clears the screen before starting playback
 * 3. Displays each frame with appropriate timing between frames
 * 4. Handles timing according to the specified FPS
 */
void draw_frames()
{
    // Get sorted list of frame files (using system ls with -V for natural sorting)
    char command[PATH_MAX + sizeof(ASCII_DIR) + sizeof("/*.txt") + sizeof(VIDEO_NAME)];
    snprintf(command, sizeof(command), "ls %s/%s*.txt | sort -V", ASCII_DIR, VIDEO_NAME);

    // Open a pipe to read the command output
    FILE *pipe = popen(command, "r");
    if (!pipe)
    {
        fatal_error("Failed to list files in directory: %s", ASCII_DIR);
    }

    char file_path[PATH_MAX];
    int frame_count = 0;

    // Clear screen before starting playback (ANSI escape sequence)
    printf("\033[2J\033[1;1H");

    // Process each frame in order
    while (fgets(file_path, sizeof(file_path), pipe) != NULL)
    {
        // Remove newline character from the file path
        file_path[strcspn(file_path, "\n")] = 0;

        // Clear screen before each frame (ANSI escape sequence)
        // \033[2J clears the screen, \033[1;1H moves cursor to top-left
        printf("\033[2J\033[1;1H");

        // Draw the current frame to the terminal
        draw_ascii_frame(file_path);
        frame_count++;

        // Calculate and implement delay between frames based on FPS
        // Convert frames per second to microseconds per frame
        int delay_us = 1000000 / atoi(FPS); 
        
        // Create timespec structure for nanosleep
        struct timespec ts = {
            .tv_sec = delay_us / 1000000,         // Seconds part
            .tv_nsec = (delay_us % 1000000) * 1000 // Nanoseconds part
        };
        
        // Pause execution for the calculated time
        nanosleep(&ts, NULL);
    }

    // Close the pipe when playback is complete
    pclose(pipe);
}

/**
 * Check if the current video has been properly extracted and is ready for playback
 * 
 * This function verifies that all necessary assets (audio and ASCII frames)
 * exist for the current video before attempting playback.
 * 
 * @return true if all video assets are available, false otherwise
 */
int video_extracted()
{
    // Define file patterns to check for audio and ASCII frame files
    char audio_file[PATH_MAX + sizeof(AUDIO_DIR) + sizeof(".mp3") + sizeof(VIDEO_NAME)] = {0};
    char ascii_file[PATH_MAX + sizeof(ASCII_DIR) + sizeof(".txt") + sizeof(VIDEO_NAME)] = {0};
    
    // Format the patterns using the current video name
    snprintf(audio_file, sizeof(audio_file), "%s.mp3", VIDEO_NAME);
    snprintf(ascii_file, sizeof(ascii_file), "%s_gray_*.txt", VIDEO_NAME);

    // Check all necessary conditions:
    // 1. Audio directory contains files
    // 2. ASCII directory contains files
    // 3. Audio directory contains the expected audio file
    // 4. ASCII directory contains matching frame files
    if (is_directory_empty(AUDIO_DIR) || 
        is_directory_empty(ASCII_DIR) || 
        !dir_contains(AUDIO_DIR, audio_file) || 
        !dir_contains(ASCII_DIR, ascii_file))
    {
        return false;  // Missing required assets
    }
    
    return true;  // All required assets are available
}

/**
 * Check if a directory contains files matching a specified pattern
 * 
 * This function examines a directory to determine if it contains any files
 * that match the specified pattern. The pattern can include wildcards
 * and is processed using fnmatch.
 * 
 * @param dir_path Path to the directory to check
 * @param file_name File name pattern to match (can include wildcards)
 * @return true if a matching file exists, false otherwise
 */
int dir_contains(const char *dir_path, const char *file_name)
{
    // Validate input parameters
    if (dir_path == NULL || file_name == NULL)
    {
        return false;
    }

    // Open the directory
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return false;  // Directory doesn't exist or can't be accessed
    }

    // Iterate through directory entries
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Check if the current entry matches the pattern
        // fnmatch allows for wildcards and pattern matching
        if (fnmatch(file_name, entry->d_name, 0) == 0)
        {
            closedir(dir);
            return true;  // Found a matching file
        }
    }

    // Clean up and return false if no match was found
    closedir(dir);
    return false;
}
/**
 * Play the ASCII video with synchronized audio
 * 
 * This function plays the ASCII video for the current VIDEO_NAME by:
 * 1. Checking if the video has been properly extracted
 * 2. Creating two child processes - one for displaying frames and one for playing audio
 * 3. Synchronizing these processes to create a complete playback experience
 * 
 * If the video hasn't been extracted (except for the default video),
 * the function will exit with an error message.
 */
void play()
{
    // Verify the video exists and has been properly extracted
    // Skip this check for the default video which may be pre-installed
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
    }
    else
    {
        // Parent process: create second child for audio playback

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
        }
        else
        {
            // Parent process: wait for both child processes to complete
            int status;
            waitpid(pid, &status, 0);   // Wait for frame display to finish
            waitpid(pid2, &status, 0);  // Wait for audio playback to finish
        }
    }
}

/**
 * Empty a directory by removing all its files and subdirectories
 * 
 * This function recursively removes all contents of the specified directory
 * without removing the directory itself. It handles both files (using unlink)
 * and subdirectories (recursively emptying then using rmdir).
 * 
 * @param dir_name Path to the directory to be emptied
 */
void empty_directory(const char *dir_name)
{
    // Validate input parameter
    if (dir_name == NULL)
    {
        fatal_error("Invalid directory name provided, path is NULL");
    }

    // Open the directory
    DIR *dir = opendir(dir_name);
    if (dir)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // Skip the special directories "." and ".."
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            // Construct full path to the current entry
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dir_name, entry->d_name);

            // Get information about the current entry
            struct stat st;
            if (stat(path, &st) == 0)
            {
                if (S_ISDIR(st.st_mode))
                {
                    // Handle subdirectory: recursively empty then remove it
                    empty_directory(path);
                    rmdir(path);
                }
                else
                {
                    // Handle regular file: delete it
                    unlink(path);
                }
            }
        }
        // Close the directory handle
        closedir(dir);
    }
}

/**
 * Create a directory if it doesn't already exist
 * 
 * This function checks if the specified directory exists, and if not,
 * creates it with standard permissions (0755 = rwxr-xr-x).
 * 
 * @param dir_name Path to the directory to create
 */
void create_dir(const char *dir_name)
{
    // Validate input parameter
    if (dir_name == NULL)
    {
        fatal_error("Invalid directory name provided, path is NULL");
    }

    // Check if directory already exists
    struct stat st = {0};
    if (stat(dir_name, &st) != 0)
    {
        // Directory doesn't exist, create it with permissions 0755 (rwxr-xr-x)
        if (mkdir(dir_name, 0755) == -1)
        {
            // Handle directory creation failure
            fatal_error("Failed to create directory: %s (Error: %s)",
                        dir_name, strerror(errno));
        }
    }
    // If directory already exists, nothing needs to be done
}
/**
 * Generate a detailed usage message with instructions and examples
 *
 * This function creates a formatted help message that describes all available
 * command-line options with their defaults and provides usage examples.
 * The returned string must be freed by the caller to prevent memory leaks.
 *
 * @param program_name The name of the program (usually argv[0])
 * @return A dynamically allocated string containing the usage message
 */
char *get_usage_msg(const char *program_name)
{
    // Allocate memory for the usage message
    char *usage = malloc(BUFFER_SIZE); // Allocate enough space for the message
    if (usage == NULL)
    {
        fatal_error("Memory allocation failed for usage message");
    }

    // Format the string with program_name and default values
    // This includes all command-line options, their descriptions, defaults, and examples
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
 * Extract audio track from the input video file
 * 
 * This function uses ffmpeg to extract the audio component from the specified
 * video file and saves it as an MP3 file. It runs ffmpeg in a child process
 * with a progress spinner displayed to the user. The audio extraction respects
 * the START_TIME and DURATION parameters.
 *
 * Dependencies: ffmpeg must be installed and accessible in the PATH
 */
void extract_audio()
{
    // Fork a child process to handle the ffmpeg execution
    pid_t pid = fork();
    if (pid == 0) // Child process
    {
        // Construct output path for the extracted audio file
        char out[PATH_MAX + sizeof(AUDIO_DIR) + sizeof("/.mp3") + sizeof(VIDEO_NAME)];
        snprintf(out, sizeof(out), AUDIO_DIR "/%s.mp3", VIDEO_NAME);
        
        // Remove existing file to prevent ffmpeg prompt about overwriting
        if (access(out, F_OK) == 0)
        {
            unlink(out); // Remove the existing file
        }
        
        // Prepare ffmpeg command arguments
        char *args[20]; // Array to hold command and arguments
        int arg_count = 0;

        // Basic ffmpeg setup with quiet logging and input file
        args[arg_count++] = "ffmpeg";
        args[arg_count++] = "-loglevel";
        args[arg_count++] = "quiet";
        args[arg_count++] = "-ss";
        args[arg_count++] = START_TIME; // Starting timestamp from config
        args[arg_count++] = "-i";
        args[arg_count++] = VIDEO_PATH; // Input video path

        // Add duration parameter if specified (non-zero)
        if (atoi(DURATION) > 0)
        {
            args[arg_count++] = "-t";
            args[arg_count++] = DURATION;
        }

        // Audio extraction parameters
        args[arg_count++] = "-vn";           // No video
        args[arg_count++] = "-acodec";       // Audio codec
        args[arg_count++] = "libmp3lame";    // Use MP3 encoder
        args[arg_count++] = "-q:a";          // Audio quality
        args[arg_count++] = "2";             // High quality (0-9, lower is better)
        args[arg_count++] = out;             // Output file path
        args[arg_count++] = NULL;            // Terminate the arguments list

        // Execute ffmpeg with the prepared arguments
        execvp("ffmpeg", args);
        _exit(EXIT_FAILURE); // Only reached if execvp fails
    }
    else if (pid < 0) // Fork failed
    {
        fatal_error("fork() failed: %s", strerror(errno));
    }
    else // Parent process
    {
        // Display spinner while ffmpeg is running
        spinner_t *sp = spinner_create("Extracting audio");
        spinner_start(sp);

        // Wait for child process to complete
        int status;
        waitpid(pid, &status, 0);

        // Stop spinner with success/failure indication
        spinner_stop(sp, WIFEXITED(status) && WEXITSTATUS(status) == 0);
        spinner_destroy(sp);

        // Handle errors if ffmpeg failed
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            // Check if the error is due to missing input file
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
 * 
 * This function uses ffmpeg to extract frames from the video file at the specified
 * FPS rate, converting them to grayscale and resizing them based on the configured
 * width. Each frame is saved as a separate PNG file in the FRAMES_DIR directory.
 * The frame extraction respects the START_TIME and DURATION parameters.
 *
 * Dependencies: ffmpeg must be installed and accessible in the PATH
 */
void extract_images_grayscale()
{
    // Fork a child process to handle the ffmpeg execution
    pid_t pid = fork();
    if (pid == 0) // Child process
    {
        // Construct output pattern for the extracted frames
        // %04d will be replaced by ffmpeg with a 4-digit frame number (0001, 0002, etc.)
        char output_pattern[PATH_MAX + sizeof(FRAMES_DIR) + sizeof("_gray_%%04d.png")];
        snprintf(output_pattern, sizeof(output_pattern),
                 "%s/%s_gray_%%04d.png", FRAMES_DIR, VIDEO_NAME);

        // Prepare ffmpeg command arguments
        char *args[20]; // Array to hold command and arguments
        int arg_count = 0;

        // Basic ffmpeg setup with quiet logging and input file
        args[arg_count++] = "ffmpeg";
        args[arg_count++] = "-loglevel";
        args[arg_count++] = "quiet";
        args[arg_count++] = "-ss";
        args[arg_count++] = START_TIME; // Starting timestamp from config
        args[arg_count++] = "-i";
        args[arg_count++] = VIDEO_PATH; // Input video path

        // Add duration parameter if specified (non-zero)
        if (atoi(DURATION) > 0)
        {
            args[arg_count++] = "-t";
            args[arg_count++] = DURATION;
        }

        // Video filter chain to:
        // 1. Set the frame rate (fps)
        // 2. Scale the width while maintaining aspect ratio (-1)
        // 3. Convert to grayscale format
        args[arg_count++] = "-vf";
        char vf[BUFFER_SIZE];
        snprintf(vf, sizeof(vf), "fps=%s,scale=%s:-1,format=gray", FPS, WIDTH);
        args[arg_count++] = vf;

        // Output pattern for the extracted frames
        args[arg_count++] = output_pattern;
        args[arg_count++] = NULL; // Terminate the arguments list

        // Execute ffmpeg with the prepared arguments
        execvp("ffmpeg", args);
        _exit(EXIT_FAILURE); // Only reached if execvp fails
    }
    else if (pid < 0) // Fork failed
    {
        fatal_error("fork() failed: %s", strerror(errno));
    }
    else // Parent process
    {
        // Display spinner while ffmpeg is running
        spinner_t *sp = spinner_create("Extracting frames");
        spinner_start(sp);

        // Wait for child process to complete
        int status;
        waitpid(pid, &status, 0);

        // Stop spinner with success/failure indication
        spinner_stop(sp, WIFEXITED(status) && WEXITSTATUS(status) == 0);
        spinner_destroy(sp);

        // Handle errors if ffmpeg failed
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            // Check if the error is due to missing input file
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
 * 
 * This function processes all PNG frames in FRAMES_DIR directory and converts them
 * to ASCII art text files using jp2a. The conversion happens in a separate
 * child process, with each frame being converted by a dedicated grandchild process.
 * A spinner is displayed during conversion to provide visual feedback to the user.
 * 
 * Process structure:
 * - Parent process: Shows spinner and waits for completion
 * - Child process: Iterates through frames and manages conversion
 * - Grandchild processes: Convert individual frames using jp2a
 * 
 * Dependencies: jp2a must be installed and accessible in the PATH
 */
void batch_convert_to_ascii()
{
    // Fork a child process to handle the conversion work
    pid_t pid = fork();
    if (pid < 0)
        fatal_error("fork() failed: %s", strerror(errno));

    if (pid == 0) // Child process
    {
        // Open the frames directory to iterate through its contents
        DIR *dir = opendir(FRAMES_DIR);
        if (dir == NULL)
        {
            fatal_error("Failed to open directory: %s", FRAMES_DIR);
        }

        // Iterate through all entries in the directory
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // Skip the special directories "." and ".."
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            // Check if the file has a .png extension
            char *ext = strrchr(entry->d_name, '.');
            if (ext == NULL || strncmp(ext, ".png", 4) != 0)
            {
                continue;
            }

            // Prepare input and output paths for the conversion
            char input_path[PATH_MAX];
            char output_path[PATH_MAX + sizeof(ASCII_DIR) + sizeof(".txt")];

            // Extract the base name from the file (without extension)
            char base_name[PATH_MAX];
            strncpy(base_name, entry->d_name, ext - entry->d_name);
            base_name[ext - entry->d_name] = '\0';

            // Construct full paths for input PNG and output ASCII file
            snprintf(input_path, sizeof(input_path), "%s/%s", FRAMES_DIR, entry->d_name);
            snprintf(output_path, sizeof(output_path), "%s/%s.txt", ASCII_DIR, base_name);

            // Fork another process to handle the conversion of this specific frame
            pid_t child_pid = fork();
            if (child_pid == 0) // Grandchild process
            {
                // Format the output argument for jp2a
                char output_arg[sizeof(output_path) + sizeof("--output=")];
                snprintf(output_arg, sizeof(output_arg), "--output=%s", output_path);

                // Execute jp2a to convert the image to ASCII
                execlp("jp2a", "jp2a", output_arg, input_path, NULL);
                _exit(EXIT_FAILURE); // Only reached if execlp fails
            }
            else // Original child process
            {
                // Wait for the conversion of this frame to complete
                int status;
                waitpid(child_pid, &status, 0);
            }
        }

        // Clean up and exit the child process
        closedir(dir);
        _exit(EXIT_SUCCESS);
    }
    else // Parent process
    {
        // Display a spinner while the conversion is in progress
        spinner_t *sp = spinner_create("Rendering ASCII art");
        spinner_start(sp);
        
        // Wait for the child process to complete all conversions
        waitpid(pid, NULL, 0);
        
        // Stop the spinner and clean up
        spinner_stop(sp, true);
        spinner_destroy(sp);
    }
}

/**
 * Checks if a directory is empty
 * 
 * This function examines the specified directory to determine if it contains
 * any files or subdirectories (excluding the special directory entries "." and "..").
 * 
 * @param dir_path Path to the directory to check
 * @return 1 if the directory is empty
 *         0 if the directory contains files or subdirectories
 *        -1 on error (NULL path or directory cannot be opened)
 */
int is_directory_empty(const char *dir_path)
{
    // Validate the input parameter
    if (dir_path == NULL)
    {
        return -1; // Invalid parameter
    }

    // Attempt to open the directory
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        return -1; // Error opening directory (may not exist or insufficient permissions)
    }

    struct dirent *entry;
    int is_empty = 1; // Assume empty until proven otherwise

    // Scan through directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip the special directory entries "." and ".."
        // Any other entry means the directory is not empty
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            is_empty = 0; // Found a file or subdirectory
            break;
        }
    }

    // Clean up and return result
    closedir(dir);
    return is_empty;
}

/**
 * Check if a directory exists at the specified path
 *
 * This function verifies whether the given path exists and is a directory.
 * It uses stat() to get file information and then checks if the path
 * represents a directory using the S_ISDIR macro.
 *
 * @param path The file path to check for directory existence
 * @return 1 if the path exists and is a directory
 *         0 if the path doesn't exist, isn't a directory, or can't be accessed
 */
int directory_exists(const char *path)
{
    struct stat st;

    // stat() returns 0 on success, -1 on error (e.g., if path doesn't exist)
    if (stat(path, &st) == 0)
    {
        // Verify the path points to a directory and not a regular file or other type
        if (S_ISDIR(st.st_mode))
        {
            return 1; // Directory exists and is accessible
        }
    }

    return 0; // Directory does not exist, is not a directory, or is not accessible
}

/**
 * Validate that a string contains only digits (0-9)
 *
 * This function checks if the given string is a valid non-negative integer
 * by verifying that it is non-empty and contains only digit characters.
 * It does not check for integer overflow or validate the numerical value.
 *
 * @param str The string to validate
 * @return 1 if the string contains only digits and is non-empty
 *         0 if the string is NULL, empty, or contains non-digit characters
 */
int is_valid_integer(const char *str)
{
    // Check for NULL or empty string
    if (str == NULL || *str == '\0')
        return 0;

    // Examine each character to ensure it's a digit (0-9)
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] < '0' || str[i] > '9')
            return 0; // Found a non-digit character
    }
    
    return 1; // All characters are digits
}

/**
 * Validate that a string represents a valid timestamp in HH:MM:SS format
 *
 * This function performs a thorough validation of a timestamp string:
 * 1. Checks the overall format (8 characters with colons at positions 2 and 5)
 * 2. Verifies all other characters are digits
 * 3. Validates the values are within valid ranges:
 *    - Hours: 00-23
 *    - Minutes: 00-59
 *    - Seconds: 00-59
 *
 * @param str The timestamp string to validate
 * @return 1 if the string represents a valid HH:MM:SS timestamp
 *         0 if the string is invalid (NULL, wrong format, or out of range values)
 */
int is_valid_timestamp(const char *str)
{
    // Check for NULL pointer
    if (str == NULL)
        return 0;

    // Verify string length and format (exactly 8 chars with colons at positions 2 and 5)
    if (strlen(str) != 8)
        return 0;
    if (str[2] != ':' || str[5] != ':')
        return 0;

    // Validate hours component (00-23)
    if (!isdigit(str[0]) || !isdigit(str[1]))
        return 0;
    int hours = (str[0] - '0') * 10 + (str[1] - '0');
    if (hours > 23)
        return 0; // Hours out of valid range

    // Validate minutes component (00-59)
    if (!isdigit(str[3]) || !isdigit(str[4]))
        return 0;
    int minutes = (str[3] - '0') * 10 + (str[4] - '0');
    if (minutes > 59)
        return 0; // Minutes out of valid range

    // Validate seconds component (00-59)
    if (!isdigit(str[6]) || !isdigit(str[7]))
        return 0;
    int seconds = (str[6] - '0') * 10 + (str[7] - '0');
    if (seconds > 59)
        return 0; // Seconds out of valid range

    return 1; // Timestamp is valid
}
