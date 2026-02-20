# Notation V Architecture

This document contains a high-level architecture overview for Notation V (Notation 5).

## Layers

### Core (`src/core/`)
Platform-agnostic domain and persistence logic.

- `NoteModel` - note data model
- `NoteStore` - in-memory note collection and observer updates
- `SearchIndex` - note filtering/search indexing
- `Storage` - local note file I/O
- `WebDAVSyncManager` - sync orchestration against configured WebDAV backend

### UI (`src/ui/`)
Qt widgets and interaction behavior.

- `MainWindow` - shell/window layout and top-level widgets
- `SearchField` - search input behavior
- `NoteList` - note listing, selection, inline rename
- `NoteEditor` - text editing, auto-save, checklist mode switching
- `CheckboxWidget` - checklist item UI and keyboard handling
- `WebDAVConfigDialog` - sync settings dialog

### Application / Platform (`src/app/`)
Composition and platform-specific integration.

- `main.cpp` - app bootstrap/wiring
- `application_controller` - coordinates store, search, selection, editor actions
- `platform/*/MenuBar.*` - OS-specific menu bar setup and menu actions

## Data Flow (simplified)

1. User edits/searches in UI widgets.
2. `ApplicationController` updates filtered results and selected note state.
3. `NoteEditor` writes changes through `Storage` (auto-save + explicit save shortcut).
4. `NoteStore` observer callbacks refresh UI models.
5. WebDAV sync is triggered through `WebDAVSyncManager` when enabled.
