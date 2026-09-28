/**
 * sero2-node — Node.js bindings for the sero2 protocol library.
 *
 * @module sero2-node
 */

// ── Load the native addon ───────────────────────────────────────

export interface Sero2NativeBindings {
  version(): string;
  [key: string]: any;
}

function loadNativeBinding(): Sero2NativeBindings {
  const prebuilt = `./sero2-node-${process.platform}-${process.arch}.node`;
  try {
    return require(prebuilt);
  } catch {
    // Fallback for local development (after running `bun run build:native`)
    return require("./build/Release/sero2-node.node");
  }
}

export const bindings: Sero2NativeBindings = loadNativeBinding();
export default bindings;
