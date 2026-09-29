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
  const local = "./build/Release/tinybuf-node.node";

  try {
    return require(prebuilt);
  } catch (prebuiltErr: any) {
    try {
      return require(local);
    } catch (localErr: any) {
      throw new Error(
        `Failed to load tinybuf-node native addon for ${process.platform}-${process.arch}.\n` +
        `  1. ${prebuilt}: ${prebuiltErr?.message || prebuiltErr}\n` +
        `  2. ${local}: ${localErr?.message || localErr}`
      );
    }
  }
}

export const bindings: TinybufNativeBindings = loadNativeBinding();
export default bindings;
