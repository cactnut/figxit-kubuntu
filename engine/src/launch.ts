import { join } from "node:path";

const quiet = { stdin: "ignore", stdout: "ignore", stderr: "ignore" } as const;

// Starts Figxit.app. On macOS that is open, in the background unless asked
// otherwise. On Linux the app folder has the same layout, and the helper runs
// in a session of its own, so Ctrl-C in the terminal that started it does not
// reach it. The helper then starts the engine.
export function launchApp(app: string, { background = true, args = [] as string[] } = {}): number {
  const command =
    process.platform === "darwin"
      ? ["open", ...(background ? ["-g"] : []), app, ...(args.length ? ["--args", ...args] : [])]
      : ["setsid", "-f", join(app, "Contents/MacOS/figxit-helper"), ...args];
  try {
    return Bun.spawnSync(command, quiet).exitCode ?? 1;
  } catch {
    return 1;
  }
}
