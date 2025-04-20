# Surprise Me

## A fun (evil) terminal‑based ASCII art video player 🎬

This is a joke project I made for fun. It converts videos into ASCII art and plays them in your terminal with synchronized audio and an evil twist—**Ctrl+C won't exit**. 😈

> **Note:** Media assets (audio tracks, image frames, and ASCII files) are **not included** in this repository. You must generate or supply your own media clips.

---

## Description

`Surprise Me`:

- **Extracts audio** from any video via `ffmpeg`.
- **Extracts frames** at your chosen FPS and dimensions.
- **Converts frames** into ASCII art with `jp2a`.
- **Plays audio** alongside ASCII frames using `ffplay`.
- **Traps SIGINT**, so you’ll need to `make kill` or close the terminal to stop it.

---

## Generating Your Own Meme Assets

To process and play your own video clip:

```bash
# Clone and build
git clone https://github.com/RamatjyotSingh/surprise-me.git
cd surprise-me
make

# Convert your video and play
./sm -i path/to/your/video.mp4 -f 10 -w 120 -t 60
```

This creates an `assets/` directory with subfolders:

- `assets/audio/`  → `.mp3` files
- `assets/frames/` → raw image frames
- `assets/ascii/`  → `.txt` ASCII art frames

Then replay by name:

```bash
./sm -p your_video_name
```

---

## Usage Options

```text
-i, --input FILE     Path to a video file to process and play
-f, --fps N          Frames per second (default: 10)
-w, --width N        Width in characters (default: 900)
-t, --height N       Height in characters (default: 600)
-s, --start TIME     Start time in HH:MM:SS format (default: 00:00:00)
-d, --duration SEC   Duration in seconds (default: full video)
-p, --play NAME      Play a previously converted video by name
-r, --reset          Delete all assets and reset settings
-h, --help           Display this help message
```

> **Important:** When using `-p`, other options (`-f`, `-w`, etc.) are ignored. Re‑run with `-i` to customize.

---

## Troubleshooting & Tips

- If you see `No ASCII assets found`, run with `-i` to generate them.
- For smaller terminals, reduce `-w` and `-t` values.
- To slow things down, lower `-f` to 5 or 3.

---

## Adding Your Own Memes

Feel free to convert any short clip (under ~10s) with:

```bash
./sm -i path/to/meme.mp4
```

Then share the ASCII files or update the collection in your own fork.

---

## Requirements

- **Code only**: No media files here.
- **Dependencies for conversion**:
  - `ffmpeg`
  - `jp2a`
  - `ffplay` (part of `ffmpeg`)

---

## License

This repository is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

*“The most beautiful thing we can experience is the mysterious...” — Einstein, if he had seen ASCII Rick Astley.*

