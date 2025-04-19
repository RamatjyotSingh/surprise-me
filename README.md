# Surprise Me

## A fun (evil) terminal-based ASCII art video player

This is a joke project I made for fun. Its sole purpose was to annoy users with Rick Roll in ASCII art - and yes, Ctrl+C won't work because I trapped SIGINT signals. You'll have to kill it manually by either shutting down the terminal or using `make kill`.

## Description

Surprise Me converts videos to ASCII art and plays them in your terminal with synchronized audio. The default behavior launches a "surprise" that you may find familiar...

## Usage

```
./sm [OPTIONS]
```

### Options:

- `-i, --input FILE`: Path to a video file to process
- `-f, --fps N`: Frames per second (default: 10)
- `-w, --width N`: Width in characters (default: 900)
- `-t, --height N`: Height in characters (default: 40)
- `-s, --start TIME`: Start time in HH:MM:SS format (default: 00:00:00)
- `-d, --duration SEC`: Duration in seconds (default: full video)
- `-p, --play NAME`: Play a previously converted video by name
- `-r, --reset`: Reset current configuration
- `-h, --help`: Display help message

## Available Videos

The following videos are pre-converted and ready to play:

- `rr` (the default surprise!)
- `dramatic_chipmunk`
- `hehe_boi` 
- `jjk`
- `mr_incred_uncanny`

To play a specific video:

```
./sm -p dramatic_chipmunk
```

## Adding New Videos

You can convert and add new videos to the collection:

```
./sm -i path/to/your/video.mp4
```

This will extract frames, convert them to ASCII art, and extract audio. The video name will be based on the filename (without extension).

## Known Bugs

- When manipulating start time (-s) and duration (-d), the program may hang or behave unexpectedly
- Some videos may not convert properly depending on their format

## WARNING

Running this program without arguments will execute the default "surprise". You won't be able to exit with Ctrl+C - you'll need to kill the terminal or use:

```
make kill
```

## Requirements

- **To play existing videos**: No external dependencies needed!
- **To add new videos or process with options**: 
  - ffmpeg
  - jp2a
  - ffplay (part of ffmpeg)

## Why Does This Exist?

Because rickrolling people in ASCII art seemed like something the world needed, and I had too much free time.