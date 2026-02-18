"""
Check ChronoLog story event count by running the chronolog_reader tool in --once
mode and counting JSON lines in the output. ChronoLog is not pub/sub; the reader
polls via playback_story() and we verify the count after the tracer has written.
"""
import argparse
import os
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(
        description="Check ChronoLog story event count using chronolog_reader"
    )
    parser.add_argument("reader_binary", help="Path to chronolog_reader executable")
    parser.add_argument(
        "expected_count",
        type=int,
        help="Minimum number of events expected (or 0 for none)",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.reader_binary) or not os.access(args.reader_binary, os.X_OK):
        print(f"Error: Reader binary not found or not executable: {args.reader_binary}")
        sys.exit(1)

    print(
        f"Checking ChronoLog story for at least {args.expected_count} events "
        f"(using {args.reader_binary})..."
    )

    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".pfw",
        delete=False,
    ) as f:
        output_path = f.name

    try:
        cmd = [
            args.reader_binary,
            "--once",
            "--output",
            output_path,
        ]
        env = os.environ.copy()
        result = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=30)

        if result.returncode != 0:
            print(f"Error: chronolog_reader failed (exit {result.returncode})")
            if result.stderr:
                print(result.stderr, file=sys.stderr)
            sys.exit(1)

        with open(output_path, "r") as f:
            lines = [line for line in f if line.strip()]

        count = len(lines)

        if args.expected_count == 0:
            if count == 0:
                print("Success: Found 0 events as expected.")
                sys.exit(0)
            print(f"Failure: Expected 0 events, found {count}")
            sys.exit(1)

        if count >= args.expected_count:
            print(f"Success: Found {count} events (>= {args.expected_count})")
            sys.exit(0)

        print(f"Failure: Expected at least {args.expected_count} events, found {count}")
        sys.exit(1)
    finally:
        if os.path.exists(output_path):
            os.unlink(output_path)


if __name__ == "__main__":
    main()
