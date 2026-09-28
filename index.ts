/**
 * tinybuf-node — Node.js bindings for the tinybuf protocol library.
 *
 * @module tinybuf-node
 */

// ── Load the native addon ───────────────────────────────────────

export interface TinybufNativeBindings {
  version(): string;
  [key: string]: any;
}

function loadNativeBinding(): TinybufNativeBindings {
  const prebuilt = `./tinybuf-node-${process.platform}-${process.arch}.node`;
  try {
    return require(prebuilt);
  } catch {
    // Fallback for local development (after running `bun run build:native`)
    return require("./build/Release/tinybuf-node.node");
  }
}

export const bindings: TinybufNativeBindings = loadNativeBinding();
export default bindings;
