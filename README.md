# Surprise Me

## A fun (evil) terminal-based ASCII art video player 🎬

This is a joke project I made for fun. Its sole purpose was to annoy users with Rick Roll in ASCII art - and yes, Ctrl+C won't work because I trapped SIGINT signals. You'll have to kill it manually by either shutting down the terminal or using `make kill`. 😈

## Description

Surprise Me converts videos to ASCII art and plays them in your terminal with synchronized audio. The default behavior launches a "surprise" that you may find familiar... (hint: it's never gonna give you up).

## The Meme Collection 🎭

The following internet classics are pre-converted and ready to play:

| Command | Meme | Description |
|---------|------|-------------|
| `./sm -p rr` | Rick Roll | Never gonna give you up! The default surprise. |
| `./sm -p dramatic_chipmunk` | Dramatic Chipmunk | That dramatic turn with intense music. |
| `./sm -p hehe_boi` | Hehe Boi | Green alien face with that famous phrase. |
| `./sm -p jjk` | Jujutsu Kaisen | Popular anime sequence in ASCII glory. |
| `./sm -p mr_incred_uncanny` | Mr. Incredible Becoming Uncanny | Watch his mood deteriorate in ASCII. |
| `./sm -p troll_face` | Troll Face | Problem? 😏 |
| `./sm -p confused_travolta` | Confused Travolta | Where is... everything? |
| `./sm -p kool_aid` | Kool-Aid Man | OH YEAH! |
| `./sm -p michael` | Michael Scott "Oh God No" | The Office moment of pure panic and dread. |
| `./sm -p monke_puppet` | Monkey Puppet | That awkward side-eye meme. |
| `./sm -p blinking` | Blinking Guy | The famous blinking reaction. |

## Usage

```
./sm [OPTIONS]
```

### Options:

- `-i, --input FILE`: Path to a video file to process
- `-f, --fps N`: Frames per second (default: 10)
- `-w, --width N`: Width in characters (default: 900)
- `-t, --height N`: Height in characters (default: 600)
- `-s, --start TIME`: Start time in HH:MM:SS format (default: 00:00:00)
- `-d, --duration SEC`: Duration in seconds (default: full video)
- `-p, --play NAME`: Play a previously converted video by name
- `-r, --reset`: Reset current configuration
- `-h, --help`: Display help message

**Important Note**: When using `-p` to play an existing video, all other options (fps, width, etc.) will be ignored. If you want to modify time or dimensions, you must use `-i` instead.

## Display Issues?

If the ASCII art appears too large or distorted for your terminal:

```
# For smaller screens, try reducing width and height:
./sm -i dramatic_chipmunk.mp4 -w 80 -t 40

# For higher quality but smaller display:
./sm -i rr.mp4 -w 120 -t 60

# For slow motion viewing (great for the dramatic chipmunk):
./sm -i dramatic_chipmunk.mp4 -f 5
```

## Adding Your Own Memes

You can convert and add new videos to the collection:

```
./sm -i path/to/your/meme.mp4
```

This will extract frames, convert them to glorious ASCII art, and extract audio. The video name will be based on the filename (without extension).

## Known Bugs

- When manipulating start time (-s) and duration (-d), the program may hang or behave unexpectedly
- Some videos may not convert properly depending on their format
- Too much ASCII art may cause sensory overload and uncontrollable laughter

## ⚠️ WARNING ⚠️

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

Because rickrolling people in ASCII art seemed like something the world needed, and I had too much free time. Also, what's better than a meme? A meme in ASCII art, of course!

---

*"The most beautiful thing we can experience is the mysterious. It is the source of all true art and science." - Albert Einstein, if he had seen ASCII Rick Astley*