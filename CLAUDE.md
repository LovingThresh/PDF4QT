# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Windows (Visual Studio / Qt Creator)

```bash
# Configure (Debug)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake

# Configure (Release)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake

# Build all
cmake --build build

# Build specific target
cmake --build build --target Pdf4QtLibCore
cmake --build build --target Pdf4QtEditor
cmake --build build --target Pdf4QtEditorPlugins
cmake --build build --target UnitTests
```

### Running Tests

```bash
# Build and run tests
cmake --build build --target UnitTests
ctest --test-dir build --output-on-failure

# Run specific test
./build/UnitTests/UnitTests --filter "test_name"
```

## Project Architecture

### Core Components

- **Pdf4QtLibCore** - PDF parsing, rendering, document manipulation, annotations
- **Pdf4QtLibGui** - Main windows, dialogs, viewer/editor controllers
- **Pdf4QtLibWidgets** - Reusable widget components
- **Pdf4QtEditor** - Main editor application
- **Pdf4QtEditorPlugins** - Plugin system (AgentPlugin, SignaturePlugin, etc.)

### AI Agent Architecture (in development)

Located in `Pdf4QtLibCore/sources/agent/`:
- `pdfagenttypes.h/cpp` - Core types (chat messages, LLM config/responses, tool calls)
- `pdfagentllmclient.h/cpp` - HTTP client for OpenAI-compatible LLM APIs
- `pdfagentfunctionregistry.h/cpp` - Tool/command registration and execution
- `pdfagentorchestrator.h/cpp` - Multi-turn conversation orchestration with tool calling

Plugin integration: `Pdf4QtEditorPlugins/AgentPlugin/`

## Key Patterns

### Qt Signal/Slot Usage
- Use `Q_OBJECT` macro in QObject-derived classes
- Signals for async events (chatFinished, toolCallFinished, etc.)
- Connections via `connect()` in constructor or setup methods

### Naming Conventions
- Classes: `PdfXxx` (e.g., `PdfDocument`, `PDFAgentOrchestrator`)
- Header files: `pdfxxx.h`
- Source files: `pdfxxx.cpp`
- Constants: `kXxx` or `XxxEnum`

### Code Organization
- Headers in `include/` or `sources/`
- Implementation in `sources/`
- Tests in `UnitTests/`

## Important Files

- `CMakeLists.txt` - Root build configuration
- `Pdf4QtLibCore/CMakeLists.txt` - Core library build
- `Pdf4QtEditorPlugins/CMakeLists.txt` - Plugin build
- `RELEASES.txt` - Version history
