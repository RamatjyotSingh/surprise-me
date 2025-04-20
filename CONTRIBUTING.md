# Contributing to Surprise Me

First of all, thank you for considering contributing to this silly but wonderful ASCII art video player! Whether you're fixing bugs, adding features, or simply improving documentation, all contributions are welcome.

## Ways to Contribute

### 1. Add New Memes

The most fun way to contribute is to add new memes to the collection:

1. Find a short, iconic video clip (ideally under 10 seconds)
2. Process it with the tool:
   ```bash
   ./sm -i path/to/meme.mp4 -w 120 -t 60
   ```
3. Submit a pull request with:
   - The processed files
   - An update to the README.md table with your new meme

### 2. Code Improvements

Want to make the code better? Here's how:

- **Bug fixes**: Find and fix issues, especially around timing and sync
- **Performance**: Improve playback performance or file processing
- **Features**: Add new features like pause/resume or playback controls

### 3. Documentation

Help improve the documentation:

- Fix typos or clarify instructions
- Add examples of interesting uses
- Create diagrams or visual guides

## Development Setup

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/surprise-me.git
   cd surprise-me
   ```

2. Install dependencies:
   ```bash
   # For Debian/Ubuntu
   sudo apt-get install ffmpeg jp2a
   
   # For macOS
   brew install ffmpeg jp2a
   ```

3. Build the project:
   ```bash
   make
   ```

4. Test your changes (please don't break the rickroll):
   ```bash
   make test
   ```

## Pull Request Process

1. Fork the repository
2. Create a branch for your feature (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to your branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Code Style

- Follow the existing C code style
- Add thorough comments for complex functionality
- Validate input parameters in functions
- Use clear, descriptive variable and function names

## License

By contributing, you agree that your contributions will be licensed under the project's MIT License.

## The Most Important Rule

Keep it fun! This project exists to surprise, entertain, and maybe slightly annoy people. Let's keep that spirit alive in everything we add to it.