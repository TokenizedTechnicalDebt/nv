# Notation V (Notation 5)

Notation V is a lightweight, keyboard-centric note taking app built for fun using the local Ollama instance and **Qwen3-coder-next**, with a few bugs smashed by GPT5 that Qwen3 struggled with.

It supports local plain-text notes and WebDAV sync, and currently works with the same WebDAV backend used by **SimpleNotesSync** on Android.

> ⚠️ **Status:** Linux is the only platform currently tested. **Windows and macOS are untested** at this time.

## Features

- Fast live search
- Implicit note creation while typing
- Auto-save (500ms debounce)
- Keyboard-first workflow
- Text notes and checklist notes
- WebDAV sync configuration

## Checklist Notes

To create a checklist note:

1. Create a normal text note.
2. Write checklist items in markdown checkbox style, one per line, for example:

```text
[x] item 1
[ ] item 2
```

3. Save the note.
4. The note will render as a checklist the next time you load/open that note.

## Requirements

- Qt 6.5+
- C++17 compiler
- CMake 3.20+

## Build (Linux)

```bash
apt install qt6-base-dev qt6-tools-dev build-essential libxkbcommon-x11-dev
cmake -B build
cmake --build build
```

## Run

```bash
./build/nv
```

## Keyboard Shortcuts

You can also view these in-app from **Help → Shortcuts**.

### Global

| Shortcut | Action |
|---|---|
| `Ctrl+L` | Focus search |
| `Ctrl+R` | Rename selected note |
| `Ctrl+S` (or `Cmd+S` on macOS) | Save current note |
| `Ctrl+Q` (or `Cmd+Q` on macOS) | Quit application |
| `Ctrl+,` (Linux/Windows) | Open Notes Directory settings |

### Edit

| Shortcut | Action |
|---|---|
| `Ctrl+Z` / `Cmd+Z` | Undo |
| `Ctrl+Y` / `Cmd+Y` | Redo |
| `Ctrl+X` / `Cmd+X` | Cut |
| `Ctrl+C` / `Cmd+C` | Copy |
| `Ctrl+V` / `Cmd+V` | Paste |
| `Ctrl+A` / `Cmd+A` | Select all |

### Search Field

| Key | Action |
|---|---|
| `Enter` | Select first match or create note |
| `Esc` | Clear search |
| `Tab` | Focus note list |

### Note List

| Key | Action |
|---|---|
| `↑` / `↓` | Move selection |
| `Enter` | Focus editor/checklist |
| `Tab` | Focus editor/checklist |

### Editor

| Key | Action |
|---|---|
| `Tab` | Focus search field |

### Checklist Editor

| Shortcut | Action |
|---|---|
| `↑` / `↓` | Move between checklist items |
| `Enter` | Add checklist item |
| `Tab` | Focus search field |
| `Ctrl+T` | Toggle current checkbox |
| `Ctrl+D` | Delete current checklist item |

## Project Docs

- Architecture and code organization: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

## License

MIT — see [`LICENSE`](LICENSE).
