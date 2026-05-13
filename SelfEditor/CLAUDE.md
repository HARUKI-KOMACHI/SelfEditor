# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SelfEditor is a Windows C++ console application built with Visual Studio 2022 (MSBuild, toolset v143, Windows SDK 10.0).

## Build Commands

```bash
# Release build (x64)
msbuild SelfEditor.sln /p:Configuration=Release /p:Platform=x64

# Debug build (x64)
msbuild SelfEditor.sln /p:Configuration=Debug /p:Platform=x64

# 32-bit variants
msbuild SelfEditor.sln /p:Configuration=Release /p:Platform=Win32
msbuild SelfEditor.sln /p:Configuration=Debug /p:Platform=Win32
```

Output lands in `x64/Release/` or `x64/Debug/` (and `Win32/...` for 32-bit).

## Project Structure

- `SelfEditor.sln` — Visual Studio solution
- `SelfEditor.vcxproj` — C++ project; add new source/header files here
- Source files go under the **ソース ファイル** (Source Files) filter (`.cpp`)
- Headers go under **ヘッダー ファイル** (Header Files) (`.h`, `.hpp`)

## Compiler Settings

- Unicode character set
- Console subsystem
- Debug: SDL checks + conformance mode enabled
- Release: whole-program optimization, COMDAT folding, reference optimization
