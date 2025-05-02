/*******************************************************************************
 * ASCII Video Player - WebAssembly Version
 *
 * This program converts video files to ASCII art and plays them in the browser
 * with synchronized audio.
 ******************************************************************************/

#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default configuration values */
#define DEFAULT_FPS 10               /* Frames per second for playback */
#define DEFAULT_WIDTH 80             /* Width of ASCII output in characters */
#define DEFAULT_HEIGHT 40            /* Height of ASCII output in characters */
#define DEFAULT_DURATION 0           /* Duration in seconds (0 means full video) */

/* Configuration structure */
typedef struct {
    int fps;
    int width;
    int height;
    double startTime;
    int duration;
    char videoName[256];
} Config;

/* Global configuration */
Config config = {
    DEFAULT_FPS,
    DEFAULT_WIDTH,
    DEFAULT_HEIGHT,
    0.0,
    DEFAULT_DURATION,
    ""
};

/* Frame data structure */
typedef struct {
    char* ascii;
    int length;
} AsciiFrame;

/* Video state */
typedef struct {
    AsciiFrame* frames;
    int frameCount;
    int currentFrame;
    int isPlaying;
} VideoState;

VideoState videoState = {NULL, 0, 0, 0};

/* Forward declarations */
void processVideo(const char* videoUrl);
void renderFrame(const char* frameData);
void startPlayback();
void stopPlayback();
int calculateGrayscale(unsigned char r, unsigned char g, unsigned char b);
char mapGrayscaleToAscii(int grayscale);

/**
 * Main initialization function called by Emscripten
 */
EMSCRIPTEN_KEEPALIVE
int initialize() {
    printf("ASCII Video Player initialized\n");
    return 1;
}

/**
 * Convert RGB pixel to ASCII character
 */
char mapGrayscaleToAscii(int grayscale) {
    // Simplified ASCII mapping (can be expanded)
    static const char* asciiChars = " .:-=+*#%@";
    int numChars = strlen(asciiChars);
    int index = grayscale * (numChars - 1) / 255;
    return asciiChars[index];
}

/**
 * Calculate grayscale value from RGB
 */
EMSCRIPTEN_KEEPALIVE
int calculateGrayscale(unsigned char r, unsigned char g, unsigned char b) {
    return (int)(0.299 * r + 0.587 * g + 0.114 * b);
}

/**
 * Process video frame to ASCII (called from JavaScript)
 */
EMSCRIPTEN_KEEPALIVE
const char* processFrame(unsigned char* imageData, int width, int height, int channels) {
    static char* asciiFrame = NULL;
    static int lastSize = 0;
    
    int asciiWidth = config.width;
    int asciiHeight = config.height;
    int newSize = (asciiWidth + 1) * asciiHeight + 1;
    
    if (asciiFrame == NULL || newSize > lastSize) {
        free(asciiFrame);
        asciiFrame = (char*)malloc(newSize);
        lastSize = newSize;
    }
    
    // Calculate sampling steps
    float stepX = (float)width / asciiWidth;
    float stepY = (float)height / asciiHeight;
    
    int asciiIndex = 0;
    
    for (int y = 0; y < asciiHeight; y++) {
        for (int x = 0; x < asciiWidth; x++) {
            int imgX = (int)(x * stepX);
            int imgY = (int)(y * stepY);
            
            // Get pixel from the source image
            int pixelIndex = (imgY * width + imgX) * channels;
            unsigned char r = imageData[pixelIndex];
            unsigned char g = imageData[pixelIndex + 1];
            unsigned char b = imageData[pixelIndex + 2];
            
            // Convert to grayscale and map to ASCII
            int gray = calculateGrayscale(r, g, b);
            asciiFrame[asciiIndex++] = mapGrayscaleToAscii(gray);
        }
        asciiFrame[asciiIndex++] = '\n';
    }
    
    asciiFrame[asciiIndex] = '\0';
    return asciiFrame;
}

/**
 * Set configuration parameters (called from JavaScript)
 */
EMSCRIPTEN_KEEPALIVE
void setConfig(int fps, int width, int height, double startTime, int duration) {
    config.fps = fps > 0 && fps <= 60 ? fps : DEFAULT_FPS;
    config.width = width > 0 ? width : DEFAULT_WIDTH;
    config.height = height > 0 ? height : DEFAULT_HEIGHT;
    config.startTime = startTime >= 0 ? startTime : 0;
    config.duration = duration >= 0 ? duration : DEFAULT_DURATION;
}

/**
 * Render animation frame (called by animation loop)
 */
void renderLoop() {
    if (!videoState.isPlaying || !videoState.frames) return;
    
    if (videoState.currentFrame < videoState.frameCount) {
        AsciiFrame frame = videoState.frames[videoState.currentFrame];
        EM_ASM({
            renderAsciiFrame(UTF8ToString($0));
        }, frame.ascii);
        
        videoState.currentFrame++;
    } else {
        // Loop or stop
        videoState.currentFrame = 0;
    }
}

/**
 * Start video playback
 */
EMSCRIPTEN_KEEPALIVE
void startPlayback() {
    if (!videoState.frames || videoState.frameCount == 0) return;
    
    videoState.currentFrame = 0;
    videoState.isPlaying = 1;
    
    // Set up rendering at the specified FPS
    emscripten_set_main_loop(renderLoop, config.fps, 1);
}

/**
 * Stop video playback
 */
EMSCRIPTEN_KEEPALIVE
void stopPlayback() {
    videoState.isPlaying = 0;
    emscripten_cancel_main_loop();
}

/**
 * Store a frame in the video state (called from JavaScript)
 */
EMSCRIPTEN_KEEPALIVE
void storeFrame(const char* asciiData) {
    // Expand array if needed
    if (videoState.frameCount == 0) {
        videoState.frames = (AsciiFrame*)malloc(sizeof(AsciiFrame) * 10);
        videoState.frameCount = 0;
    } else if (videoState.frameCount % 10 == 0) {
        videoState.frames = (AsciiFrame*)realloc(
            videoState.frames,
            sizeof(AsciiFrame) * (videoState.frameCount + 10)
        );
    }
    
    // Store the frame
    int len = strlen(asciiData);
    videoState.frames[videoState.frameCount].ascii = (char*)malloc(len + 1);
    strcpy(videoState.frames[videoState.frameCount].ascii, asciiData);
    videoState.frames[videoState.frameCount].length = len;
    videoState.frameCount++;
}

/**
 * Clean up resources
 */
EMSCRIPTEN_KEEPALIVE
void cleanup() {
    if (videoState.frames) {
        for (int i = 0; i < videoState.frameCount; i++) {
            free(videoState.frames[i].ascii);
        }
        free(videoState.frames);
        videoState.frames = NULL;
    }
    videoState.frameCount = 0;
    videoState.currentFrame = 0;
    videoState.isPlaying = 0;
}