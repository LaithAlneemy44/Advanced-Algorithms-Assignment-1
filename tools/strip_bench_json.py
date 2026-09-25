"""Remove machine-identifying fields from Google Benchmark JSON before commit.

Google Benchmark records the host name and the executable's full path, which
includes the Windows user name. The rest of the context, such as CPU, caches,
date and library build type, is kept because it documents the environment.

Usage:
    python tools/strip_bench_json.py FILE.json [FILE.json ...]
"""

import json
import pathlib
import sys

PRIVATE_FIELDS = ("host_name", "executable")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for name in sys.argv[1:]:
        path = pathlib.Path(name)
        data = json.loads(path.read_text())
        for field in PRIVATE_FIELDS:
            data["context"].pop(field, None)
        path.write_text(json.dumps(data, indent=2))
        print(f"stripped {path}")
