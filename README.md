# tinybuf-node

Node.js native N-API bindings for the tinybuf C++ protocol library.

## Prerequisites

- Node.js ≥ 18 or [Bun](https://bun.sh)
- `node-gyp` build toolchain (Python 3, C++17 compiler, `make`)

## Install & build

```bash
bun install    # or npm install
bun run build  # or npm run build
```

Build tasks:
- `bun run build:native`: Rebuild the C++ N-API addon with `node-gyp`
- `bun run build:ts`: Compile TypeScript files with `bun build`
- `bun run build`: Run native rebuild + TypeScript build
- `bun run electron-rebuild`: Rebuild native addon for Electron

## Project structure

- `src/tinybuf-node.cpp`: C++ N-API implementation wrapping tinybuf
- `include/`: Header files for the underlying tinybuf C++ library
- `index.ts`: TypeScript entrypoint and type definitions
- `binding.gyp`: node-gyp build configuration
