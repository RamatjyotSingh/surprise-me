#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#include <direct.h>
#include <time.h>
#include <ctype.h>

// Define path constants
#define ASSETS_DIR "assets"
#define AUDIO_DIR "assets\\audio"
#define ASCII_DIR "assets\\ascii"
#define FRAMES_DIR "assets\\frames"

// Define defaults
#define DEFAULT_FPS  "10"
#define DEFAULT_WIDTH  "900"
#define DEFAULT_HEIGHT "600"
#define DEFAULT_START_TIME "00:00:00"
#define DEFAULT_VIDEO_PATH "rr.mp4"
#define DEFAULT_VIDEO_NAME "rr"
#define DEFAULT_DURATION  "0"

#define BUFFER_SIZE 1024
#define PATH_MAX 260  // MAX_PATH in Windows

char VIDEO_PATH[PATH_MAX] = DEFAULT_VIDEO_PATH;
char* FPS = DEFAULT_FPS;
char* WIDTH = DEFAULT_WIDTH;
char* HEIGHT = DEFAULT_HEIGHT;
char* START_TIME = DEFAULT_START_TIME;
char* DURATION  = DEFAULT_DURATION;
char VIDEO_NAME[PATH_MAX] = DEFAULT_VIDEO_NAME;

// Global flag for handling Ctrl+C
static volatile int interrupt_received = 0;

// Windows Ctrl+C handler
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT) {
        interrupt_received = 1;
        return TRUE;
    }
    return FALSE;
}

// Function prototypes
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

// Windows-specific console functions
void clear_screen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, ' ', dwConSize, coordScreen, &cCharsWritten);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, coordScreen);
}

// Windows version of user message functions
void user_error(const char *format, ...) {
    HANDLE hConsole = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD originalAttrs;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    originalAttrs = consoleInfo.wAttributes;
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    fprintf(stderr, "Error: ");
    SetConsoleTextAttribute(hConsole, originalAttrs);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void user_fatal(const char *format, ...) {
    HANDLE hConsole = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD originalAttrs;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    originalAttrs = consoleInfo.wAttributes;
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY | FOREGROUND_BLUE);
    fprintf(stderr, "Fatal: ");
    SetConsoleTextAttribute(hConsole, originalAttrs);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    
    exit(EXIT_FAILURE);
}

void user_warning(const char *format, ...) {
    HANDLE hConsole = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD originalAttrs;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    originalAttrs = consoleInfo.wAttributes;
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    fprintf(stderr, "Warning: ");
    SetConsoleTextAttribute(hConsole, originalAttrs);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void user_info(const char *format, ...) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD originalAttrs;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    originalAttrs = consoleInfo.wAttributes;
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    printf("Info: ");
    SetConsoleTextAttribute(hConsole, originalAttrs);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void user_success(const char *format, ...) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD originalAttrs;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    originalAttrs = consoleInfo.wAttributes;
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("Success: ");
    SetConsoleTextAttribute(hConsole, originalAttrs);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void user_prompt(const char *format, ...) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD originalAttrs;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    originalAttrs = consoleInfo.wAttributes;
    
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY);
    printf("? ");
    SetConsoleTextAttribute(hConsole, originalAttrs);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(" ");
    fflush(stdout);
}

// Windows Process Runner - replacement for fork/exec
int run_process(const char* command, int wait_for_completion) {
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    // Create a new string buffer for the command
    char* cmd_copy = _strdup(command);
    
    // Create the process
    if (!CreateProcess(NULL, cmd_copy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmd_copy);
        return 0;
    }
    
    free(cmd_copy);
    
    // Wait for process to complete if requested
    if (wait_for_completion) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exit_code == 0);
    }
    
    // Don't wait, just return success
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 1;
}

// Set defaults
void set_defaults() {
    FPS = DEFAULT_FPS;
    WIDTH = DEFAULT_WIDTH;
    HEIGHT = DEFAULT_HEIGHT;
    START_TIME = DEFAULT_START_TIME;
    DURATION = DEFAULT_DURATION;
    VIDEO_PATH[0] = '\0';
    VIDEO_NAME[0] = '\0';
}

int main(int argc, char *argv[]) {
    // Set up Ctrl+C handler
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    
    int opts_given = 0;
    int new_file = 0;
    
    // Process command line arguments
    // Note: Windows doesn't have getopt, so we'll do simple parsing
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) {
            if (++i < argc) {
                opts_given++;
                new_file = 1;
                strncpy(VIDEO_PATH, argv[i], sizeof(VIDEO_PATH)-1);
            }
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fps") == 0) {
            if (++i < argc) {
                opts_given++;
                if (!is_valid_integer(argv[i])) {
                    user_fatal("Invalid fps value. Must be a positive integer.");
                }
                int fps_value = atoi(argv[i]);
                if (fps_value <= 0 || fps_value > 60) {
                    user_fatal("FPS must be between 1 and 60.");
                }
                FPS = argv[i];
            }
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) {
            if (++i < argc) {
                opts_given++;
                if (!is_valid_integer(argv[i])) {
                    user_fatal("Invalid width value. Must be a positive integer.");
                }
                int width_value = atoi(argv[i]);
                if (width_value <= 0) {
                    user_fatal("Width must be positive.");
                }
                WIDTH = argv[i];
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--height") == 0) {
            if (++i < argc) {
                opts_given++;
                if (!is_valid_integer(argv[i])) {
                    user_fatal("Invalid height value. Must be a positive integer.");
                }
                HEIGHT = argv[i];
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--start") == 0) {
            if (++i < argc) {
                opts_given++;
                if (!is_valid_timestamp(argv[i])) {
                    user_fatal("Invalid start time. Format must be HH:MM:SS");
                }
                START_TIME = argv[i];
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--duration") == 0) {
            if (++i < argc) {
                opts_given++;
                if (!is_valid_integer(argv[i])) {
                    user_fatal("Invalid duration. Must be a positive integer in seconds.");
                }
                DURATION = argv[i];
            }
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--reset") == 0) {
            opts_given++;
            user_warning("This will delete all extracted files and reset settings.");
            reset();
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--play") == 0) {
            if (++i < argc) {
                strncpy(VIDEO_NAME, argv[i], sizeof(VIDEO_NAME)-1);
                VIDEO_NAME[sizeof(VIDEO_NAME)-1] = '\0';
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            user_error("%s", get_usage_msg(argv[0]));
            exit(EXIT_SUCCESS);
        } else {
            user_error("Invalid option provided.");
            exit(EXIT_FAILURE);
        }
    }

    if (opts_given) {
        if (new_file) {
            char *base = strrchr(VIDEO_PATH, '\\');
            if (!base) base = strrchr(VIDEO_PATH, '/');  // Support both slashes
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

// Implementation of Windows-specific versions of the functions
void reset() {
    char response;
    int valid_response = 0;

    while (!valid_response) {
        user_prompt("Are you sure? (y/n)");
        response = getchar();
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
        
        if (response == 'y' || response == 'Y' || response == 'n' || response == 'N') {
            valid_response = 1;
        } else {
            user_error("Please enter 'y' or 'n'");
        }
    }

    if (response == 'y' || response == 'Y') {
        user_info("Resetting all directories...");
        empty_directory(AUDIO_DIR);
        empty_directory(ASCII_DIR);
        empty_directory(FRAMES_DIR);
        set_defaults();
        user_success("Reset completed successfully!");
        exit(EXIT_SUCCESS);
    } else {
        user_info("Operation cancelled.");
        exit(EXIT_SUCCESS);
    }
}

void setup() {
    if(VIDEO_PATH[0] == '\0') {
        user_fatal("Invalid path provided for video extraction, path is NULL");
    }
    create_dir(ASSETS_DIR);
    create_dir(ASCII_DIR);
    create_dir(AUDIO_DIR);
    create_dir(FRAMES_DIR);

    extract_audio();
    extract_images_grayscale();
    batch_convert_to_ascii();
}

void extract_audio() {
    char command[BUFFER_SIZE * 2];
    char out[PATH_MAX + sizeof(AUDIO_DIR) + sizeof("\\\\") + sizeof(VIDEO_NAME) + sizeof(".mp3")];
    snprintf(out, sizeof(out), "%s\\%s.mp3", AUDIO_DIR, VIDEO_NAME);
    
    // Check if output file exists and delete it
    if (_access(out, 0) == 0) {
        DeleteFile(out);
    }
    
    // Build ffmpeg command
    snprintf(command, sizeof(command),
             "ffmpeg -loglevel quiet -ss %s -i \"%s\" %s -vn -acodec libmp3lame -q:a 2 \"%s\"",
             START_TIME, VIDEO_PATH,
             (atoi(DURATION) > 0 ? "-t " : ""), (atoi(DURATION) > 0 ? DURATION : ""),
             out);
    
    user_info("Extracting audio...");
    if (!run_process(command, 1)) {
        if (_access(VIDEO_PATH, 0) != 0) {
            user_fatal("Video file not found: %s", VIDEO_PATH);
        } else {
            user_fatal("Failed to extract audio");
        }
    }
    user_success("Audio extraction complete");
}

void extract_images_grayscale() {
    char command[BUFFER_SIZE * 2];
    char output_pattern[PATH_MAX + sizeof(FRAMES_DIR) + sizeof("\\\\") + sizeof(VIDEO_NAME) + sizeof("_gray_%%04d.png")];
    snprintf(output_pattern, sizeof(output_pattern),
             "%s\\%s_gray_%%04d.png", FRAMES_DIR, VIDEO_NAME);
    
    // Build ffmpeg command
    snprintf(command, sizeof(command),
             "ffmpeg -loglevel quiet -ss %s -i \"%s\" %s -vf \"fps=%s,scale=%s:-1,format=gray\" \"%s\"",
             START_TIME, VIDEO_PATH,
             (atoi(DURATION) > 0 ? "-t " : ""), (atoi(DURATION) > 0 ? DURATION : ""),
             FPS, WIDTH, output_pattern);
    
    user_info("Extracting frames...");
    if (!run_process(command, 1)) {
        if (_access(VIDEO_PATH, 0) != 0) {
            user_fatal("Video file not found: %s", VIDEO_PATH);
        } else {
            user_fatal("Failed to extract frames");
        }
    }
    user_success("Frame extraction complete");
}

void batch_convert_to_ascii() {
    user_info("Converting frames to ASCII...");
    
    WIN32_FIND_DATA findData;
    char searchPath[PATH_MAX];
    snprintf(searchPath, sizeof(searchPath), "%s\\*.png", FRAMES_DIR);
    
    HANDLE hFind = FindFirstFile(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        user_fatal("No frames found to convert");
        return;
    }
    
    do {
        // Skip directories
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        
        // Check if file has .png extension
        char *ext = strrchr(findData.cFileName, '.');
        if (!ext || _stricmp(ext, ".png") != 0) {
            continue;
        }
        
        // Create input and output paths
        char input_path[PATH_MAX];
        char output_path[PATH_MAX];
        char base_name[PATH_MAX];
        
        strncpy(base_name, findData.cFileName, ext - findData.cFileName);
        base_name[ext - findData.cFileName] = '\0';
        
        snprintf(input_path, sizeof(input_path), "%s\\%s", FRAMES_DIR, findData.cFileName);
        snprintf(output_path, sizeof(output_path), "%s\\%s.txt", ASCII_DIR, base_name);
        
        // Create jp2a command
        char command[BUFFER_SIZE * 2];
        snprintf(command, sizeof(command), "jp2a --output=\"%s\" \"%s\"", output_path, input_path);
        
        // Run jp2a
        run_process(command, 1);
        
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    user_success("ASCII conversion complete");
}

void play() {
    if (!video_extracted() && strcmp(VIDEO_NAME, DEFAULT_VIDEO_NAME) != 0) {
        user_fatal("%s doesn't exist, try inserting a new one with -i <video_path>", VIDEO_NAME);
    }
    
    // Start audio in a separate thread
    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)play_audio, NULL, 0, NULL);
    
    // Draw frames in main thread
    draw_frames();
    
    // Wait for audio thread to complete
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
}

void play_audio() {
    char audio_file[PATH_MAX + sizeof(AUDIO_DIR) + sizeof("\\") + sizeof(VIDEO_NAME) + sizeof(".mp3")];
    snprintf(audio_file, sizeof(audio_file), "%s\\%s.mp3", AUDIO_DIR, VIDEO_NAME);
    
    // Use ShellExecuteEx to open with the system's default player
    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = "open";
    sei.lpFile = audio_file;
    sei.lpParameters = NULL;
    sei.nShow = SW_HIDE;  // Hide the player window
    
    if (!ShellExecuteEx(&sei)) {
        DWORD error = GetLastError();
        user_error("Failed to play audio with default player. Error code: %lu", error);
        return;
    }
    
    // The process handle is available in sei.hProcess if needed for later
}

void draw_frames() {
    // Windows equivalent of the Linux draw_frames function
    char search_pattern[PATH_MAX];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\%s*.txt", ASCII_DIR, VIDEO_NAME);
    
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(search_pattern, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        user_fatal("No frames found for %s", VIDEO_NAME);
        return;
    }
    
    // Create an array to store file paths
    char** files = NULL;
    int fileCount = 0;
    int capacity = 0;
    
    do {
        // Skip directories
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        
        // Expand array if needed
        if (fileCount >= capacity) {
            capacity = capacity == 0 ? 128 : capacity * 2;
            char** newFiles = (char**)realloc(files, capacity * sizeof(char*));
            if (!newFiles) {
                user_fatal("Memory allocation failed");
            }
            files = newFiles;
        }
        
        // Add file to array
        char* fullPath = (char*)malloc(PATH_MAX);
        snprintf(fullPath, PATH_MAX, "%s\\%s", ASCII_DIR, findData.cFileName);
        files[fileCount++] = fullPath;
        
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    
    // Sort files (simple bubble sort)
    for (int i = 0; i < fileCount - 1; i++) {
        for (int j = 0; j < fileCount - i - 1; j++) {
            if (strcmp(files[j], files[j + 1]) > 0) {
                char* temp = files[j];
                files[j] = files[j + 1];
                files[j + 1] = temp;
            }
        }
    }
    
    // Clear screen before starting
    clear_screen();
    
    // Draw each frame
    for (int i = 0; i < fileCount; i++) {
        // Clear screen before each frame
        clear_screen();
        
        // Draw the frame
        draw_ascii_frame(files[i]);
        
        // Add delay
        int delay_ms = 1000 / atoi(FPS);
        Sleep(delay_ms);
        
        // Free memory
        free(files[i]);
    }
    
    // Free array
    free(files);
}

void draw_ascii_frame(const char *frame_path) {
    if (frame_path == NULL) {
        user_fatal("Invalid frame path provided, path is NULL");
    }

    FILE *file = fopen(frame_path, "r");
    if (file == NULL) {
        user_fatal("Failed to open file: %s", frame_path);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file) != NULL) {
        printf("%s", line);
    }

    fclose(file);
    printf("\n");
}

char* get_usage_msg(const char *program_name) {
    char *usage = malloc(BUFFER_SIZE);
    if (usage == NULL) {
        user_fatal("Memory allocation failed for usage message");
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

void create_dir(const char *dir_name) {
    if (dir_name == NULL) {
        user_fatal("Invalid directory name provided, path is NULL");
    }
    
    // Check if directory exists
    if (_access(dir_name, 0) != 0) {
        // Create directory
        if (_mkdir(dir_name) != 0) {
            user_fatal("Failed to create directory: %s (Error: %s)",
                    dir_name, strerror(errno));
        }
    }
}

void empty_directory(const char *dir_name) {
    if (dir_name == NULL) {
        user_fatal("Invalid directory name provided, path is NULL");
    }
    
    WIN32_FIND_DATA findData;
    char searchPath[PATH_MAX];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", dir_name);
    
    HANDLE hFind = FindFirstFile(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;  // Directory doesn't exist or is empty
    }
    
    do {
        // Skip "." and ".." directories
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s\\%s", dir_name, findData.cFileName);
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recursively remove subdirectory
            empty_directory(path);
            RemoveDirectory(path);
        } else {
            // Remove file
            DeleteFile(path);
        }
        
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
}

int directory_exists(const char *path) {
    DWORD attrs = GetFileAttributes(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && 
            (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

int is_directory_empty(const char *dir_path) {
    if (dir_path == NULL) {
        return -1;
    }
    
    WIN32_FIND_DATA findData;
    char searchPath[PATH_MAX];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", dir_path);
    
    HANDLE hFind = FindFirstFile(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 1;  // Directory doesn't exist or is empty
    }
    
    int isEmpty = 1;
    do {
        // Skip "." and ".." directories
        if (strcmp(findData.cFileName, ".") != 0 && 
            strcmp(findData.cFileName, "..") != 0) {
            isEmpty = 0;
            break;
        }
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    return isEmpty;
}

int dir_contains(const char *dir_path, const char *pattern) {
    if (dir_path == NULL || pattern == NULL) {
        return 0;
    }
    
    WIN32_FIND_DATA findData;
    char searchPath[PATH_MAX];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", dir_path);
    
    HANDLE hFind = FindFirstFile(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    do {
        // Simple pattern matching (Windows doesn't have fnmatch)
        // This is a simplified version that handles '*' wildcards
        const char *p = pattern;
        const char *n = findData.cFileName;
        int matches = 1;
        
        while (*p && *n && matches) {
            if (*p == '*') {
                p++;
                if (!*p) return 1;  // Pattern ends with *, it's a match
                
                // Find next matching character
                while (*n && *n != *p) n++;
                if (!*n) matches = 0;  // No match found
            } else if (*p == *n) {
                p++;
                n++;
            } else {
                matches = 0;
            }
        }
        
        if (matches && (!*p || (*p == '*' && !*(p+1))) && !*n) {
            FindClose(hFind);
            return 1;
        }
        
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    return 0;
}

int video_extracted() {
    char audio_file[PATH_MAX] = {0};
    char ascii_file[PATH_MAX] = {0};
    snprintf(audio_file, sizeof(audio_file), "%s.mp3", VIDEO_NAME);
    snprintf(ascii_file, sizeof(ascii_file), "%s_gray_*.txt", VIDEO_NAME);

    if (is_directory_empty(AUDIO_DIR) || 
        is_directory_empty(ASCII_DIR) || 
        !dir_contains(AUDIO_DIR, audio_file) || 
        !dir_contains(ASCII_DIR, ascii_file)) {
        return 0;
    }
    return 1;
}

int is_valid_integer(const char* str) {
    if (str == NULL || *str == '\0') return 0;
    
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') return 0;
    }
    return 1;
}

int is_valid_timestamp(const char* str) {
    if (str == NULL) return 0;
    
    if (strlen(str) != 8) return 0;
    if (str[2] != ':' || str[5] != ':') return 0;
    
    if (!isdigit(str[0]) || !isdigit(str[1])) return 0;
    int hours = (str[0] - '0') * 10 + (str[1] - '0');
    if (hours > 23) return 0;
    
    if (!isdigit(str[3]) || !isdigit(str[4])) return 0;
    int minutes = (str[3] - '0') * 10 + (str[4] - '0');
    if (minutes > 59) return 0;
    
    if (!isdigit(str[6]) || !isdigit(str[7])) return 0;
    int seconds = (str[6] - '0') * 10 + (str[7] - '0');
    if (seconds > 59) return 0;
    
    return 1;
}